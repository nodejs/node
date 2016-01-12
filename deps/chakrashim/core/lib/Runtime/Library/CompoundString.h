//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // -------------------------------------------------------------------------------------------------------------------------
    // Storage
    // -------------------------------------------------------------------------------------------------------------------------
    //
    // CompoundString uses available buffer space to directly store characters or pointers, or to pack information such as a
    // substring's start index and length. It is optimized for concatenation. A compound string begins in direct character mode,
    // where it appends characters directly to the buffers. When a somewhat larger string is concatenated, the compound string
    // switches to pointer mode and records the direct character length. From that point onwards, only pointers or packed
    // information is stored in the buffers. Each piece of packed information is stored as a pointer with the lowest bit tagged.
    //
    // A compound string may have several chained Block objects, each with a buffer allocated inline with the block. The
    // compound string references only the last block in the chain (to save space), and each block references its previous block
    // in the chain. As a consequence, during flattening, blocks are iterated backwards and flattening is also done backwards.
    //
    // -------------------------------------------------------------------------------------------------------------------------
    // Using as a character-only string builder
    // -------------------------------------------------------------------------------------------------------------------------
    //
    // Using the AppendChars set of functions requires that the compound string is in direct character mode, and forces it to
    // remain in direct character mode by appending all characters directly to the buffer. Those functions can be used to build
    // a string like a typical character-only string builder. Flattening is much faster when in direct character mode, and the
    // AppendChars set of functions also get to omit the check to see if the compound string is in direct character mode, but it
    // is at the cost of having to append all characters even in the case of appending large strings, instead of just appending
    // a pointer.
    //
    // -------------------------------------------------------------------------------------------------------------------------
    // Appending
    // -------------------------------------------------------------------------------------------------------------------------
    //
    // The compound string and builder have simple Append and AppendChars functions that delegate to a set of AppendGeneric
    // functions that do the actual work. AppendGeneric functions are templatized and their implementation is shared between the
    // compound string and builder.
    //
    // After determining how to append, the AppendGeneric functions call a TryAppendGeneric function that will perform the
    // append if there is enough space in the last block's buffer. If there is no space, the AppendGeneric functions call a
    // AppendSlow function. In a compound string, the AppendSlow function grows the buffer or creates a new chained block, and
    // performs the append. In a builder, the AppendSlow function creates the compound string and delegates to it from that
    // point onwards.
    //
    // -------------------------------------------------------------------------------------------------------------------------
    // Buffer sharing and ownership
    // -------------------------------------------------------------------------------------------------------------------------
    //
    // Multiple compound string objects may reference and use the same buffers. Cloning a compound string creates a new object
    // that references the same buffer. However, only one compound string may own the last block at any given time, since the
    // last block's buffer is mutable through concatenation. Compound string objects that don't own the last block keep track of
    // the character length of the buffer in their last block info (BlockInfo object), since the block's length may be changed
    // by the block's owner.
    //
    // When a concatenation operation is performed on a compound string that does not own the last block, it will first need to
    // take ownership of that block. So, it is necessary to call PrepareForAppend() before the first append operation for a
    // compound string whose buffers may be shared. Taking ownership of a block is done by either resizing the block (and hence
    // copying the buffer up to the point to which it is used), or shallow-cloning the last block and chaining a new block to
    // it. Shallow cloning copies the block's metadata but does not copy the buffer. The character length of the clone is set to
    // the length of the portion of the buffer that is used by the compound string. The cloned block references the original
    // block that owns the buffer, for access to the buffer. Once a new block is chained to it, it then becomes the last block,
    // effectively making the clone immutable by the compound string.
    //
    // -------------------------------------------------------------------------------------------------------------------------
    // Last block info (BlockInfo object)
    // -------------------------------------------------------------------------------------------------------------------------
    //
    // Blocks are only created once chaining begins. Until then, the buffer is allocated directly and stored in the last block
    // info. The buffer is resized until it reaches a threshold, and upon the final resize before chaining begins, a block is
    // created. From that point onwards, blocks are no longer resized and only chained, although a new chained block may be
    // larger than the previous block.
    //
    // The last block info is also used to serve as a cache for information in the last block. Since the last block is where
    // concatenation occurs, it is significantly beneficial to prevent having to dereference the last block to get to its
    // information. So, the BlockInfo object representing the last block info caches the last block's information and only it is
    // used during append operations. Only when space runs out, is the actual last block updated with information from the last
    // block info. As a consequence of this and the fact that multiple compound strings may share blocks, the last block's
    // character length may not be up-to-date, or may not be relevant to the compound string querying it, so it should never be
    // queried except in specific cases where it is guaranteed to be correct.
    //
    // -------------------------------------------------------------------------------------------------------------------------
    // Builder
    // -------------------------------------------------------------------------------------------------------------------------
    //
    // The builder uses stack-allocated space for the initial buffer. It may perform better in some scenarios, but the tradeoff
    // is that it is at the cost of an additional check per append.
    //
    // It typically performs better in cases where the number of concatenations is highly unpredictable and may range from just
    // a few to a large number:
    //     - For few concatenations, the final compound string's buffer will be the minimum size necessary, so it helps by
    //       saving space, and as a result, performing a faster allocation
    //     - For many concatenations, the use of stack space reduces the number of allocations that would otherwise be necessary
    //       to grow the buffer

    class CompoundString sealed : public LiteralString // vtable will be switched to LiteralString's vtable after flattening
    {
        #pragma region CompoundString::Block
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    private:
        class Block
        {
        public:
            static const uint ChainSizeThreshold;
        private:
            static const uint MaxChainedBlockSize;

        private:
            Block *const bufferOwner;
            CharCount charLength;
            CharCount charCapacity;
            const Block *const previous;

        private:
            Block(const CharCount charCapacity, const Block *const previous);
            Block(const void *const buffer, const CharCount charLength, const CharCount charCapacity);
            Block(const Block &other, const CharCount usedCharLength);

        public:
            static Block *New(const uint size, const Block *const previous, Recycler *const recycler);
            static Block *New(const void *const buffer, const CharCount usedCharLength, const bool reserveMoreSpace, Recycler *const recycler);
            Block *Clone(const CharCount usedCharLength, Recycler *const recycler) const;

        private:
            static CharCount CharCapacityFromSize(const uint size);
            static uint SizeFromCharCapacity(const CharCount charCapacity);

        private:
            static CharCount PointerAlign(const CharCount charLength);
        public:
            static const wchar_t *Chars(const void *const buffer);
            static wchar_t *Chars(void *const buffer);
            static void *const *Pointers(const void *const buffer);
            static void **Pointers(void *const buffer);
            static CharCount PointerCapacityFromCharCapacity(const CharCount charCapacity);
            static CharCount CharCapacityFromPointerCapacity(const CharCount pointerCapacity);
            static CharCount PointerLengthFromCharLength(const CharCount charLength);
            static CharCount CharLengthFromPointerLength(const CharCount pointerLength);
            static uint SizeFromUsedCharLength(const CharCount usedCharLength);

        public:
            static bool ShouldAppendChars(const CharCount appendCharLength, const uint additionalSizeForPointerAppend = 0);

        public:
            const void *Buffer() const;
            void *Buffer();
            const Block *Previous() const;

        public:
            const wchar_t *Chars() const;
            wchar_t *Chars();
            CharCount CharLength() const;
            void SetCharLength(const CharCount charLength);
            CharCount CharCapacity() const;

        public:
            void *const *Pointers() const;
            void **Pointers();
            CharCount PointerLength() const;
            CharCount PointerCapacity() const;

        private:
            static uint GrowSize(const uint size);
            static uint GrowSizeForChaining(const uint size);
        public:
            Block *Chain(Recycler *const recycler);

        private:
            PREVENT_COPY(Block);
        };

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        #pragma endregion

        #pragma region CompoundString::BlockInfo
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    private:
        class BlockInfo
        {
        private:
            void *buffer;
            CharCount charLength;
            CharCount charCapacity;

        public:
            BlockInfo();
            BlockInfo(Block *const block);

        public:
            wchar_t *Chars() const;
            CharCount CharLength() const;
            void SetCharLength(const CharCount charLength);
            CharCount CharCapacity() const;

        public:
            void **Pointers() const;
            CharCount PointerLength() const;
            void SetPointerLength(const CharCount pointerLength);
            CharCount PointerCapacity() const;

        public:
            static CharCount AlignCharCapacityForAllocation(const CharCount charCapacity);
            static CharCount GrowCharCapacity(const CharCount charCapacity);
            static bool ShouldAllocateBuffer(const CharCount charCapacity);
            void AllocateBuffer(const CharCount charCapacity, Recycler *const recycler);
            Block *CopyBuffer(const void *const buffer, const CharCount usedCharLength, const bool reserveMoreSpace, Recycler *const recycler);
            Block *Resize(Recycler *const recycler);
            static size_t GetOffsetOfCharLength() { return offsetof(BlockInfo, charLength); }
            static size_t GetOffsetOfCharCapacity() { return offsetof(BlockInfo, charCapacity); }
            static size_t GetOffsetOfBuffer() { return offsetof(BlockInfo, buffer); }

        public:
            void CopyFrom(Block *const block);
            void CopyTo(Block *const block);
            void Unreference();
        };

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        #pragma endregion

        #pragma region CompoundString::Builder
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    public:
        template<CharCount MinimumCharCapacity>
        class Builder
        {
        private:
            // Array size needs to be a constant expression. This expression is equivalent to
            // Block::PointerLengthFromCharLength(MinimumCharCapacity), and generates a pointer capacity that equates to a char
            // capacity that is >= MinimumCharCapacity.
            void *buffer[
                (
                    (MinimumCharCapacity + sizeof(void *) / sizeof(wchar_t) - 1) &
                    ~(sizeof(void *) / sizeof(wchar_t) - 1)
                ) / (sizeof(void *) / sizeof(wchar_t))];

            CharCount stringLength;
            CharCount charLength;
            CharCount directCharLength;
            CompoundString *compoundString;
            ScriptContext *const scriptContext;

        #if DBG
            bool isFinalized;
        #endif

        public:
            Builder(ScriptContext *const scriptContext);

        private:
            bool IsFinalized() const;
            bool HasOnlyDirectChars() const;
            void SwitchToPointerMode();
            bool OwnsLastBlock() const;
            const wchar_t *GetAppendStringBuffer(JavascriptString *const s) const;
            ScriptContext *GetScriptContext() const;
            JavascriptLibrary *GetLibrary() const;

        private:
            wchar_t *LastBlockChars();
            CharCount LastBlockCharLength() const;
            void SetLastBlockCharLength(const CharCount charLength);
            CharCount LastBlockCharCapacity() const;

        private:
            void **LastBlockPointers();
            CharCount LastBlockPointerLength() const;
            void SetLastBlockPointerLength(const CharCount pointerLength);
            CharCount LastBlockPointerCapacity() const;

        private:
            CharCount GetLength() const;
            void SetLength(const CharCount stringLength);

        private:
            void AppendSlow(const wchar_t c);
            void AppendSlow(JavascriptString *const s);
            void AppendSlow(__in_xcount(appendCharLength) const wchar_t *const s, const CharCount appendCharLength);
            void AppendSlow(JavascriptString *const s, void *const packedSubstringInfo, void *const packedSubstringInfo2, const CharCount appendCharLength);

        public:
            void Append(const wchar_t c);
            void AppendChars(const wchar_t c);
            void Append(JavascriptString *const s);
            void AppendChars(JavascriptString *const s);
            void Append(JavascriptString *const s, const CharCount startIndex, const CharCount appendCharLength);
            void AppendChars(JavascriptString *const s, const CharCount startIndex, const CharCount appendCharLength);
            template<CharCount AppendCharLengthPlusOne> void Append(const wchar_t (&s)[AppendCharLengthPlusOne], const bool isCppLiteral = true);
            template<CharCount AppendCharLengthPlusOne> void AppendChars(const wchar_t (&s)[AppendCharLengthPlusOne], const bool isCppLiteral = true);
            void Append(__in_xcount(appendCharLength) const wchar_t *const s, const CharCount appendCharLength);
            void AppendChars(__in_xcount(appendCharLength) const wchar_t *const s, const CharCount appendCharLength);
            template<class TValue, class FConvertToString> void Append(const TValue &value, const CharCount maximumAppendCharLength, const FConvertToString ConvertToString);
            template<class TValue, class FConvertToString> void AppendChars(const TValue &value, const CharCount maximumAppendCharLength, const FConvertToString ConvertToString);

        private:
            CompoundString *CreateCompoundString(const bool reserveMoreSpace) const;

        public:
            JavascriptString *ToString();

            friend CompoundString;

            PREVENT_COPY(Builder);
        };

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        #pragma endregion

        #pragma region CompoundString
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    private:
        BlockInfo lastBlockInfo;
        CharCount directCharLength;
        bool ownsLastBlock;
        Block *lastBlock;

    private:
        CompoundString(const CharCount initialCharCapacity, JavascriptLibrary *const library);
        CompoundString(const CharCount initialBlockSize, const bool allocateBlock, JavascriptLibrary *const library);
        CompoundString(const CharCount stringLength, const CharCount directCharLength, const void *const buffer, const CharCount usedCharLength, const bool reserveMoreSpace, JavascriptLibrary *const library);
        CompoundString(CompoundString &other, const bool forAppending);

    public:
        static CompoundString *NewWithCharCapacity(const CharCount initialCharCapacity, JavascriptLibrary *const library);
        static CompoundString *NewWithPointerCapacity(const CharCount initialPointerCapacity, JavascriptLibrary *const library);
    private:
        static CompoundString *NewWithBufferCharCapacity(const CharCount initialCharCapacity, JavascriptLibrary *const library);
        static CompoundString *NewWithBlockSize(const CharCount initialBlockSize, JavascriptLibrary *const library);
        static CompoundString *New(const CharCount stringLength, const CharCount directCharLength, const void *const buffer, const CharCount usedCharLength, const bool reserveMoreSpace, JavascriptLibrary *const library);
    public:
        CompoundString *Clone(const bool forAppending);
        static CompoundString * JitClone(CompoundString * cs);
        static CompoundString * JitCloneForAppending(CompoundString * cs);
    public:
        static bool Is(RecyclableObject *const object);
        static bool Is(const Var var);
        static CompoundString *FromVar(RecyclableObject *const object);
        static CompoundString *FromVar(const Var var);
        static size_t GetOffsetOfOwnsLastBlock() { return offsetof(CompoundString, ownsLastBlock); }
        static size_t GetOffsetOfDirectCharLength() { return offsetof(CompoundString, directCharLength); }
        static size_t GetOffsetOfLastBlockInfo() { return offsetof(CompoundString, lastBlockInfo); }
        static size_t GetOffsetOfLastBlockInfoCharLength() { return CompoundString::BlockInfo::GetOffsetOfCharLength(); }
        static size_t GetOffsetOfLastBlockInfoCharCapacity() { return CompoundString::BlockInfo::GetOffsetOfCharCapacity(); }
        static size_t GetOffsetOfLastBlockInfoBuffer() { return CompoundString::BlockInfo::GetOffsetOfBuffer(); }

    public:
        static JavascriptString *GetImmutableOrScriptUnreferencedString(JavascriptString *const s);
        static bool ShouldAppendChars(const CharCount appendCharLength);

    private:
        bool HasOnlyDirectChars() const;
        void SwitchToPointerMode();
        bool OwnsLastBlock() const;
        const wchar_t *GetAppendStringBuffer(JavascriptString *const s) const;

    private:
        wchar_t *LastBlockChars() const;
        CharCount LastBlockCharLength() const;
        void SetLastBlockCharLength(const CharCount charLength);
        CharCount LastBlockCharCapacity() const;

    private:
        void **LastBlockPointers() const;
        CharCount LastBlockPointerLength() const;
        void SetLastBlockPointerLength(const CharCount pointerLength);
        CharCount LastBlockPointerCapacity() const;

    private:
        static void PackSubstringInfo(const CharCount startIndex, const CharCount length, void * *const packedSubstringInfoRef, void * *const packedSubstringInfo2Ref);
    public:
        static bool IsPackedInfo(void *const pointer);
        static void UnpackSubstringInfo(void *const pointer, void *const pointer2, CharCount *const startIndexRef, CharCount *const lengthRef);

    private:
        template<class String> static bool TryAppendGeneric(const wchar_t c, String *const toString);
        template<class String> static bool TryAppendGeneric(JavascriptString *const s, const CharCount appendCharLength, String *const toString);
        template<class String> static bool TryAppendFewCharsGeneric(__in_xcount(appendCharLength) const wchar_t *const s, const CharCount appendCharLength, String *const toString);
        template<class String> static bool TryAppendGeneric(__in_xcount(appendCharLength) const wchar_t *const s, const CharCount appendCharLength, String *const toString);
        template<class String> static bool TryAppendGeneric(JavascriptString *const s, void *const packedSubstringInfo, void *const packedSubstringInfo2, const CharCount appendCharLength, String *const toString);

    private:
        template<class String> static void AppendGeneric(const wchar_t c, String *const toString, const bool appendChars);
        template<class String> static void AppendGeneric(JavascriptString *const s, String *const toString, const bool appendChars);
        template<class String> static void AppendGeneric(JavascriptString *const s, const CharCount startIndex, const CharCount appendCharLength, String *const toString, const bool appendChars);
        template<CharCount AppendCharLengthPlusOne, class String> static void AppendGeneric(const wchar_t (&s)[AppendCharLengthPlusOne], const bool isCppLiteral, String *const toString, const bool appendChars);
        template<class String> static void AppendGeneric(__in_xcount(appendCharLength) const wchar_t *const s, const CharCount appendCharLength, String *const toString, const bool appendChars);
        template<class TValue, class FConvertToString, class String> static void AppendGeneric(const TValue &value, CharCount maximumAppendCharLength, const FConvertToString ConvertToString, String *const toString, const bool appendChars);

    private:
        void AppendSlow(const wchar_t c);
        void AppendSlow(JavascriptString *const s);
        void AppendSlow(__in_xcount(appendCharLength) const wchar_t *const s, const CharCount appendCharLength);
        void AppendSlow(JavascriptString *const s, void *const packedSubstringInfo, void *const packedSubstringInfo2, const CharCount appendCharLength);

    public:
        void PrepareForAppend();
        void Append(const wchar_t c);
        void AppendChars(const wchar_t c);
        void Append(JavascriptString *const s);
        void AppendChars(JavascriptString *const s);
        void Append(JavascriptString *const s, const CharCount startIndex, const CharCount appendCharLength);
        void AppendChars(JavascriptString *const s, const CharCount startIndex, const CharCount appendCharLength);
        template<CharCount AppendCharLengthPlusOne> void Append(const wchar_t (&s)[AppendCharLengthPlusOne], const bool isCppLiteral = true);
        template<CharCount AppendCharLengthPlusOne> void AppendChars(const wchar_t (&s)[AppendCharLengthPlusOne], const bool isCppLiteral = true);
        void Append(__in_xcount(appendCharLength) const wchar_t *const s, const CharCount appendCharLength);
        void AppendChars(__in_xcount(appendCharLength) const wchar_t *const s, const CharCount appendCharLength);
        void AppendCharsSz(__in_z const wchar_t *const s);
        template<class TValue, class FConvertToString> void Append(const TValue &value, const CharCount maximumAppendCharLength, const FConvertToString ConvertToString);
        template<class TValue, class FConvertToString> void AppendChars(const TValue &value, const CharCount maximumAppendCharLength, const FConvertToString ConvertToString);

    private:
        void Grow();
        void TakeOwnershipOfLastBlock();

    private:
        void Unreference();
    public:
        virtual const wchar_t *GetSz() override sealed;
        using JavascriptString::Copy;
        virtual void CopyVirtual(_Out_writes_(m_charLength) wchar_t *const buffer, StringCopyInfoStack &nestedStringTreeCopyInfos, const byte recursionDepth) override sealed;
        virtual bool IsTree() const override sealed;

    protected:
        DEFINE_VTABLE_CTOR(CompoundString, LiteralString);
        DECLARE_CONCRETE_STRING_CLASS;

    private:
        PREVENT_COPY(CompoundString);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        #pragma endregion
    };

    #pragma region CompoundString::Builder definition
    #ifndef CompoundStringJsDiag
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<CharCount MinimumCharCapacity>
    CompoundString::Builder<MinimumCharCapacity>::Builder(ScriptContext *const scriptContext)
        : stringLength(0),
        charLength(0),
        directCharLength(static_cast<CharCount>(-1)),
        compoundString(nullptr),
        scriptContext(scriptContext)
    #if DBG
        , isFinalized(false)
    #endif
    {
        CompileAssert(MinimumCharCapacity != 0);
        Assert(LastBlockCharCapacity() >= MinimumCharCapacity);
    }

    template<CharCount MinimumCharCapacity>
    bool CompoundString::Builder<MinimumCharCapacity>::IsFinalized() const
    {
    #if DBG
        return isFinalized;
    #else
        return false;
    #endif
    }

    template<CharCount MinimumCharCapacity>
    bool CompoundString::Builder<MinimumCharCapacity>::HasOnlyDirectChars() const
    {
        return directCharLength == static_cast<CharCount>(-1);
    }

    template<CharCount MinimumCharCapacity>
    void CompoundString::Builder<MinimumCharCapacity>::SwitchToPointerMode()
    {
        Assert(HasOnlyDirectChars());

        directCharLength = charLength;

        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(L"CompoundString::SwitchToPointerMode() - directCharLength = %u\n", directCharLength);
            Output::Flush();
        }
    }

    template<CharCount MinimumCharCapacity>
    bool CompoundString::Builder<MinimumCharCapacity>::OwnsLastBlock() const
    {
        return true;
    }

    template<CharCount MinimumCharCapacity>
    __inline const wchar_t *CompoundString::Builder<MinimumCharCapacity>::GetAppendStringBuffer(
        JavascriptString *const s) const
    {
        Assert(s);

        return s->GetString();
    }

    template<CharCount MinimumCharCapacity>
    ScriptContext *CompoundString::Builder<MinimumCharCapacity>::GetScriptContext() const
    {
        return scriptContext;
    }

    template<CharCount MinimumCharCapacity>
    JavascriptLibrary *CompoundString::Builder<MinimumCharCapacity>::GetLibrary() const
    {
        return scriptContext->GetLibrary();
    }

    template<CharCount MinimumCharCapacity>
    wchar_t *CompoundString::Builder<MinimumCharCapacity>::LastBlockChars()
    {
        return Block::Chars(buffer);
    }

    template<CharCount MinimumCharCapacity>
    CharCount CompoundString::Builder<MinimumCharCapacity>::LastBlockCharLength() const
    {
        return charLength;
    }

    template<CharCount MinimumCharCapacity>
    void CompoundString::Builder<MinimumCharCapacity>::SetLastBlockCharLength(const CharCount charLength)
    {
        this->charLength = charLength;
    }

    template<CharCount MinimumCharCapacity>
    CharCount CompoundString::Builder<MinimumCharCapacity>::LastBlockCharCapacity() const
    {
        return Block::CharCapacityFromPointerCapacity(LastBlockPointerCapacity());
    }

    template<CharCount MinimumCharCapacity>
    void **CompoundString::Builder<MinimumCharCapacity>::LastBlockPointers()
    {
        return Block::Pointers(buffer);
    }

    template<CharCount MinimumCharCapacity>
    CharCount CompoundString::Builder<MinimumCharCapacity>::LastBlockPointerLength() const
    {
        return Block::PointerLengthFromCharLength(charLength);
    }

    template<CharCount MinimumCharCapacity>
    void CompoundString::Builder<MinimumCharCapacity>::SetLastBlockPointerLength(const CharCount pointerLength)
    {
        charLength = Block::CharLengthFromPointerLength(pointerLength);
    }

    template<CharCount MinimumCharCapacity>
    CharCount CompoundString::Builder<MinimumCharCapacity>::LastBlockPointerCapacity() const
    {
        return _countof(buffer);
    }

    template<CharCount MinimumCharCapacity>
    CharCount CompoundString::Builder<MinimumCharCapacity>::GetLength() const
    {
        return stringLength;
    }

    template<CharCount MinimumCharCapacity>
    void CompoundString::Builder<MinimumCharCapacity>::SetLength(const CharCount stringLength)
    {
        if(!IsValidCharCount(stringLength))
            Throw::OutOfMemory();
        this->stringLength = stringLength;
    }

    template<CharCount MinimumCharCapacity>
    void CompoundString::Builder<MinimumCharCapacity>::AppendSlow(const wchar_t c)
    {
        Assert(!this->compoundString);
        CompoundString *const compoundString = CreateCompoundString(true);
        this->compoundString = compoundString;
        const bool appended =
            HasOnlyDirectChars()
                ? TryAppendGeneric(c, compoundString)
                : TryAppendGeneric(GetLibrary()->GetCharStringCache().GetStringForChar(c), 1, compoundString);
        Assert(appended);
    }

    template<CharCount MinimumCharCapacity>
    void CompoundString::Builder<MinimumCharCapacity>::AppendSlow(JavascriptString *const s)
    {
        Assert(!this->compoundString);
        CompoundString *const compoundString = CreateCompoundString(true);
        this->compoundString = compoundString;
        const bool appended = TryAppendGeneric(s, s->GetLength(), compoundString);
        Assert(appended);
    }

    template<CharCount MinimumCharCapacity>
    void CompoundString::Builder<MinimumCharCapacity>::AppendSlow(
        __in_xcount(appendCharLength) const wchar_t *const s,
        const CharCount appendCharLength)
    {
        // Even though CreateCompoundString() will create a compound string with some additional space reserved for appending,
        // the amount of space available may still not be enough, so need to check and fall back to the slow path as well
        Assert(!this->compoundString);
        CompoundString *const compoundString = CreateCompoundString(true);
        this->compoundString = compoundString;
        if(TryAppendGeneric(s, appendCharLength, compoundString))
            return;
        compoundString->AppendSlow(s, appendCharLength);
    }

    template<CharCount MinimumCharCapacity>
    void CompoundString::Builder<MinimumCharCapacity>::AppendSlow(
        JavascriptString *const s,
        void *const packedSubstringInfo,
        void *const packedSubstringInfo2,
        const CharCount appendCharLength)
    {
        Assert(!this->compoundString);
        CompoundString *const compoundString = CreateCompoundString(true);
        this->compoundString = compoundString;
        const bool appended = TryAppendGeneric(s, packedSubstringInfo, packedSubstringInfo2, appendCharLength, compoundString);
        Assert(appended);
    }

    template<CharCount MinimumCharCapacity>
    __inline void CompoundString::Builder<MinimumCharCapacity>::Append(const wchar_t c)
    {
        if(!compoundString)
        {
            AppendGeneric(c, this, false);
            return;
        }

        compoundString->Append(c);
    }

    template<CharCount MinimumCharCapacity>
    __inline void CompoundString::Builder<MinimumCharCapacity>::AppendChars(const wchar_t c)
    {
        if(!compoundString)
        {
            AppendGeneric(c, this, true);
            return;
        }

        compoundString->AppendChars(c);
    }

    template<CharCount MinimumCharCapacity>
    __inline void CompoundString::Builder<MinimumCharCapacity>::Append(JavascriptString *const s)
    {
        if(!compoundString)
        {
            AppendGeneric(s, this, false);
            return;
        }

        compoundString->Append(s);
    }

    template<CharCount MinimumCharCapacity>
    __inline void CompoundString::Builder<MinimumCharCapacity>::AppendChars(JavascriptString *const s)
    {
        if(!compoundString)
        {
            AppendGeneric(s, this, true);
            return;
        }

        compoundString->AppendChars(s);
    }

    template<CharCount MinimumCharCapacity>
    __inline void CompoundString::Builder<MinimumCharCapacity>::Append(
        JavascriptString *const s,
        const CharCount startIndex,
        const CharCount appendCharLength)
    {
        if(!compoundString)
        {
            AppendGeneric(s, startIndex, appendCharLength, this, false);
            return;
        }

        compoundString->Append(s, startIndex, appendCharLength);
    }

    template<CharCount MinimumCharCapacity>
    __inline void CompoundString::Builder<MinimumCharCapacity>::AppendChars(
        JavascriptString *const s,
        const CharCount startIndex,
        const CharCount appendCharLength)
    {
        if(!compoundString)
        {
            AppendGeneric(s, startIndex, appendCharLength, this, true);
            return;
        }

        compoundString->AppendChars(s, startIndex, appendCharLength);
    }

    template<CharCount MinimumCharCapacity>
    template<CharCount AppendCharLengthPlusOne>
    __inline void CompoundString::Builder<MinimumCharCapacity>::Append(
        const wchar_t (&s)[AppendCharLengthPlusOne],
        const bool isCppLiteral)
    {
        if(!compoundString)
        {
            AppendGeneric(s, isCppLiteral, this, false);
            return;
        }

        compoundString->Append(s, isCppLiteral);
    }

    template<CharCount MinimumCharCapacity>
    template<CharCount AppendCharLengthPlusOne>
    __inline void CompoundString::Builder<MinimumCharCapacity>::AppendChars(
        const wchar_t (&s)[AppendCharLengthPlusOne],
        const bool isCppLiteral)
    {
        if(!compoundString)
        {
            AppendGeneric(s, isCppLiteral, this, true);
            return;
        }

        compoundString->AppendChars(s, isCppLiteral);
    }

    template<CharCount MinimumCharCapacity>
    __inline void CompoundString::Builder<MinimumCharCapacity>::Append(
        __in_xcount(appendCharLength) const wchar_t *const s,
        const CharCount appendCharLength)
    {
        if(!compoundString)
        {
            AppendGeneric(s, appendCharLength, this, false);
            return;
        }

        compoundString->Append(s, appendCharLength);
    }

    template<CharCount MinimumCharCapacity>
    __inline void CompoundString::Builder<MinimumCharCapacity>::AppendChars(
        __in_xcount(appendCharLength) const wchar_t *const s,
        const CharCount appendCharLength)
    {
        if(!compoundString)
        {
            AppendGeneric(s, appendCharLength, this, true);
            return;
        }

        compoundString->AppendChars(s, appendCharLength);
    }

    template<CharCount MinimumCharCapacity>
    template<class TValue, class FConvertToString>
    __inline void CompoundString::Builder<MinimumCharCapacity>::Append(
        const TValue &value,
        const CharCount maximumAppendCharLength,
        const FConvertToString ConvertToString)
    {
        if(!compoundString)
        {
            AppendGeneric(value, maximumAppendCharLength, ConvertToString, this, false);
            return;
        }

        compoundString->Append(value, maximumAppendCharLength, ConvertToString);
    }

    template<CharCount MinimumCharCapacity>
    template<class TValue, class FConvertToString>
    __inline void CompoundString::Builder<MinimumCharCapacity>::AppendChars(
        const TValue &value,
        const CharCount maximumAppendCharLength,
        const FConvertToString ConvertToString)
    {
        if(!compoundString)
        {
            AppendGeneric(value, maximumAppendCharLength, ConvertToString, this, true);
            return;
        }

        compoundString->AppendChars(value, maximumAppendCharLength, ConvertToString);
    }

    template<CharCount MinimumCharCapacity>
    CompoundString *CompoundString::Builder<MinimumCharCapacity>::CreateCompoundString(const bool reserveMoreSpace) const
    {
        return
            CompoundString::New(
                stringLength,
                directCharLength,
                buffer,
                charLength,
                reserveMoreSpace,
                this->GetLibrary());
    }

    template<CharCount MinimumCharCapacity>
    __inline JavascriptString *CompoundString::Builder<MinimumCharCapacity>::ToString()
    {
    #if DBG
        // Should not append to the builder after this function is called
        isFinalized = true;
    #endif

        CompoundString *const compoundString = this->compoundString;
        if(compoundString)
            return compoundString;

        switch(stringLength)
        {
            default:
                return CreateCompoundString(false);

            case 0:
                return this->GetLibrary()->GetEmptyString();

            case 1:
                Assert(HasOnlyDirectChars());
                Assert(LastBlockCharLength() == 1);

                return this->GetLibrary()->GetCharStringCache().GetStringForChar(LastBlockChars()[0]);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #endif
    #pragma endregion

    #pragma region CompoundString template member definitions
    #ifndef CompoundStringJsDiag
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<class String>
    __inline bool CompoundString::TryAppendGeneric(const wchar_t c, String *const toString)
    {
        Assert(toString);
        Assert(!toString->IsFinalized());
        Assert(toString->OwnsLastBlock());
        Assert(toString->HasOnlyDirectChars());

        const CharCount blockCharLength = toString->LastBlockCharLength();
        if(blockCharLength < toString->LastBlockCharCapacity())
        {
            toString->LastBlockChars()[blockCharLength] = c;
            toString->SetLength(toString->GetLength() + 1);
            toString->SetLastBlockCharLength(blockCharLength + 1);
            return true;
        }
        return false;
    }

    template<class String>
    __inline bool CompoundString::TryAppendGeneric(
        JavascriptString *const s,
        const CharCount appendCharLength,
        String *const toString)
    {
        Assert(s);
        Assert(appendCharLength == s->GetLength());
        Assert(toString);
        Assert(!toString->IsFinalized());
        Assert(toString->OwnsLastBlock());
        Assert(!toString->HasOnlyDirectChars());

        const CharCount blockPointerLength = toString->LastBlockPointerLength();
        if(blockPointerLength < toString->LastBlockPointerCapacity())
        {
            toString->LastBlockPointers()[blockPointerLength] = GetImmutableOrScriptUnreferencedString(s);
            toString->SetLength(toString->GetLength() + appendCharLength);
            toString->SetLastBlockPointerLength(blockPointerLength + 1);
            return true;
        }
        return false;
    }

    template<class String>
    __inline bool CompoundString::TryAppendFewCharsGeneric(
        __in_xcount(appendCharLength) const wchar_t *const s,
        const CharCount appendCharLength,
        String *const toString)
    {
        Assert(s);
        Assert(toString);
        Assert(!toString->IsFinalized());
        Assert(toString->OwnsLastBlock());
        Assert(toString->HasOnlyDirectChars());

        const CharCount blockCharLength = toString->LastBlockCharLength();
        if(appendCharLength <= toString->LastBlockCharCapacity() - blockCharLength)
        {
            const wchar_t *appendCharBuffer = s;
            wchar_t *charBuffer = &toString->LastBlockChars()[blockCharLength];
            const wchar_t *const charBufferEnd = charBuffer + appendCharLength;
            for(; charBuffer != charBufferEnd; ++appendCharBuffer, ++charBuffer)
                *charBuffer = *appendCharBuffer;
            toString->SetLength(toString->GetLength() + appendCharLength);
            toString->SetLastBlockCharLength(blockCharLength + appendCharLength);
            return true;
        }
        return false;
    }

    template<class String>
    __inline bool CompoundString::TryAppendGeneric(
        __in_xcount(appendCharLength) const wchar_t *const s,
        const CharCount appendCharLength,
        String *const toString)
    {
        Assert(s);
        Assert(toString);
        Assert(!toString->IsFinalized());
        Assert(toString->OwnsLastBlock());
        Assert(toString->HasOnlyDirectChars());

        const CharCount blockCharLength = toString->LastBlockCharLength();
        if(appendCharLength <= toString->LastBlockCharCapacity() - blockCharLength)
        {
            CopyHelper(&toString->LastBlockChars()[blockCharLength], s, appendCharLength);
            toString->SetLength(toString->GetLength() + appendCharLength);
            toString->SetLastBlockCharLength(blockCharLength + appendCharLength);
            return true;
        }
        return false;
    }

    template<class String>
    __inline bool CompoundString::TryAppendGeneric(
        JavascriptString *const s,
        void *const packedSubstringInfo,
        void *const packedSubstringInfo2,
        const CharCount appendCharLength,
        String *const toString)
    {
        Assert(s);
        Assert(packedSubstringInfo);
        Assert(appendCharLength <= s->GetLength());
        Assert(toString);
        Assert(!toString->IsFinalized());
        Assert(toString->OwnsLastBlock());
        Assert(!toString->HasOnlyDirectChars());

        const CharCount blockPointerLength = toString->LastBlockPointerLength();
        const CharCount appendPointerLength = 2 + !!packedSubstringInfo2;
        if(blockPointerLength < toString->LastBlockPointerCapacity() - (appendPointerLength - 1))
        {
            void * *const pointers = toString->LastBlockPointers();
            pointers[blockPointerLength] = GetImmutableOrScriptUnreferencedString(s);
            if(packedSubstringInfo2)
                pointers[blockPointerLength + 1] = packedSubstringInfo2;
            pointers[blockPointerLength + (appendPointerLength - 1)] = packedSubstringInfo;
            toString->SetLength(toString->GetLength() + appendCharLength);
            toString->SetLastBlockPointerLength(blockPointerLength + appendPointerLength);
            return true;
        }
        return false;
    }

    template<class String>
    __inline void CompoundString::AppendGeneric(const wchar_t c, String *const toString, const bool appendChars)
    {
        Assert(toString);
        Assert(!toString->IsFinalized());
        Assert(toString->OwnsLastBlock());
        Assert(!(appendChars && !toString->HasOnlyDirectChars()));

        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(L"CompoundString::AppendGeneric('%c', appendChars = %s)\n", c, appendChars ? L"true" : L"false");
            Output::Flush();
        }

        if(appendChars || toString->HasOnlyDirectChars()
                ? TryAppendGeneric(c, toString)
                : TryAppendGeneric(toString->GetLibrary()->GetCharStringCache().GetStringForChar(c), 1, toString))
        {
            return;
        }
        toString->AppendSlow(c);
    }

    template<class String>
    __inline void CompoundString::AppendGeneric(
        JavascriptString *const s,
        String *const toString,
        const bool appendChars)
    {
        Assert(s);
        Assert(toString);
        Assert(!toString->IsFinalized());
        Assert(toString->OwnsLastBlock());
        Assert(!(appendChars && !toString->HasOnlyDirectChars()));

        const CharCount appendCharLength = s->GetLength();
        if(appendCharLength == 0)
            return;

        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"CompoundString::AppendGeneric(JavascriptString *s = \"%.8s%s\", appendCharLength = %u, appendChars = %s)\n",
                s->IsFinalized() ? s->GetString() : L"",
                !s->IsFinalized() || appendCharLength > 8 ? L"..." : L"",
                appendCharLength,
                appendChars ? L"true" : L"false");
            Output::Flush();
        }

        if(appendChars || toString->HasOnlyDirectChars())
        {
            if(appendCharLength == 1)
            {
                const wchar_t c = toString->GetAppendStringBuffer(s)[0];
                if(TryAppendGeneric(c, toString))
                    return;
                toString->AppendSlow(c);
                return;
            }

            if(appendChars || Block::ShouldAppendChars(appendCharLength))
            {
                const wchar_t *const appendBuffer = toString->GetAppendStringBuffer(s);
                if(appendChars
                        ? TryAppendGeneric(appendBuffer, appendCharLength, toString)
                        : TryAppendFewCharsGeneric(appendBuffer, appendCharLength, toString))
                {
                    return;
                }
                toString->AppendSlow(appendBuffer, appendCharLength);
                return;
            }

            toString->SwitchToPointerMode();
        }

        if(TryAppendGeneric(s, appendCharLength, toString))
            return;
        toString->AppendSlow(s);
    }

    template<class String>
    __inline void CompoundString::AppendGeneric(
        JavascriptString *const s,
        const CharCount startIndex,
        const CharCount appendCharLength,
        String *const toString,
        const bool appendChars)
    {
        Assert(s);
        Assert(startIndex <= s->GetLength());
        Assert(appendCharLength <= s->GetLength() - startIndex);
        Assert(toString);
        Assert(!toString->IsFinalized());
        Assert(toString->OwnsLastBlock());
        Assert(!(appendChars && !toString->HasOnlyDirectChars()));

        if(appendCharLength == 0)
            return;

        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"CompoundString::AppendGeneric(JavascriptString *s = \"%.*s%s\", startIndex = %u, appendCharLength = %u, appendChars = %s)\n",
                min(static_cast<CharCount>(8), appendCharLength),
                s->IsFinalized() ? &s->GetString()[startIndex] : L"",
                !s->IsFinalized() || appendCharLength > 8 ? L"..." : L"",
                startIndex,
                appendCharLength,
                appendChars ? L"true" : L"false");
            Output::Flush();
        }

        if(appendChars || toString->HasOnlyDirectChars())
        {
            if(appendCharLength == 1)
            {
                const wchar_t c = toString->GetAppendStringBuffer(s)[startIndex];
                if(TryAppendGeneric(c, toString))
                    return;
                toString->AppendSlow(c);
                return;
            }

            if(appendChars || Block::ShouldAppendChars(appendCharLength, sizeof(void *)))
            {
                const wchar_t *const appendBuffer = &toString->GetAppendStringBuffer(s)[startIndex];
                if(appendChars
                        ? TryAppendGeneric(appendBuffer, appendCharLength, toString)
                        : TryAppendFewCharsGeneric(appendBuffer, appendCharLength, toString))
                {
                    return;
                }
                toString->AppendSlow(appendBuffer, appendCharLength);
                return;
            }

            toString->SwitchToPointerMode();
        }

        if(appendCharLength == 1)
        {
            JavascriptString *const js =
                toString->GetLibrary()->GetCharStringCache().GetStringForChar(toString->GetAppendStringBuffer(s)[startIndex]);
            if(TryAppendGeneric(js, 1, toString))
                return;
            toString->AppendSlow(js);
            return;
        }

        void *packedSubstringInfo, *packedSubstringInfo2;
        PackSubstringInfo(startIndex, appendCharLength, &packedSubstringInfo, &packedSubstringInfo2);
        if(TryAppendGeneric(s, packedSubstringInfo, packedSubstringInfo2, appendCharLength, toString))
            return;
        toString->AppendSlow(s, packedSubstringInfo, packedSubstringInfo2, appendCharLength);
    }

    template<CharCount AppendCharLengthPlusOne, class String>
    __inline void CompoundString::AppendGeneric(
        const wchar_t (&s)[AppendCharLengthPlusOne],
        const bool isCppLiteral,
        String *const toString,
        const bool appendChars)
    {
        CompileAssert(AppendCharLengthPlusOne != 0);
        Assert(s);
        Assert(s[AppendCharLengthPlusOne - 1] == L'\0');
        Assert(toString);
        Assert(!toString->IsFinalized());
        Assert(toString->OwnsLastBlock());
        Assert(!(appendChars && !toString->HasOnlyDirectChars()));

        if(AppendCharLengthPlusOne == 1)
            return;
        if(AppendCharLengthPlusOne == 2)
        {
            AppendGeneric(s[0], toString, appendChars);
            return;
        }

        const CharCount appendCharLength = AppendCharLengthPlusOne - 1;
        if(!isCppLiteral)
        {
            AppendGeneric(s, appendCharLength, toString, appendChars);
            return;
        }

        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"CompoundString::AppendGeneric(C++ literal \"%.8s%s\", appendCharLength = %u, appendChars = %s)\n",
                s,
                appendCharLength > 8 ? L"..." : L"",
                appendCharLength,
                appendChars ? L"true" : L"false");
            Output::Flush();
        }

        if(appendChars || toString->HasOnlyDirectChars())
        {
            if(appendChars || Block::ShouldAppendChars(appendCharLength, sizeof(LiteralString)))
            {
                if(appendChars
                        ? TryAppendGeneric(s, appendCharLength, toString)
                        : TryAppendFewCharsGeneric(s, appendCharLength, toString))
                {
                    return;
                }
                toString->AppendSlow(s, appendCharLength);
                return;
            }

            toString->SwitchToPointerMode();
        }

        JavascriptString *const js = toString->GetLibrary()->CreateStringFromCppLiteral(s);
        if(TryAppendGeneric(js, appendCharLength, toString))
            return;
        toString->AppendSlow(js);
    }

    template<class String>
    __inline void CompoundString::AppendGeneric(
        __in_xcount(appendCharLength) const wchar_t *const s,
        const CharCount appendCharLength,
        String *const toString,
        const bool appendChars)
    {
        Assert(s);
        Assert(toString);
        Assert(!toString->IsFinalized());
        Assert(toString->OwnsLastBlock());
        Assert(!(appendChars && !toString->HasOnlyDirectChars()));

        if(appendCharLength == 0)
            return;

        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"CompoundString::AppendGeneric(wchar_t *s = \"%.8s%s\", appendCharLength = %u, appendChars = %s)\n",
                s,
                appendCharLength > 8 ? L"..." : L"",
                appendCharLength,
                appendChars ? L"true" : L"false");
            Output::Flush();
        }

        if(appendChars || toString->HasOnlyDirectChars())
        {
            if(appendCharLength == 1)
            {
                const wchar_t c = s[0];
                if(TryAppendGeneric(c, toString))
                    return;
                toString->AppendSlow(c);
                return;
            }

            // Skip the check for Block::ShouldAppendChars because the string buffer has to be copied anyway
            if(TryAppendGeneric(s, appendCharLength, toString))
                return;
            toString->AppendSlow(s, appendCharLength);
            return;
        }

        JavascriptString *const js = LiteralString::NewCopyBuffer(s, appendCharLength, toString->GetScriptContext());
        if(TryAppendGeneric(js, appendCharLength, toString))
            return;
        toString->AppendSlow(js);
    }

    template<class TValue, class FConvertToString, class String>
    __inline void CompoundString::AppendGeneric(
        const TValue &value,
        CharCount maximumAppendCharLength,
        const FConvertToString ConvertToString,
        String *const toString,
        const bool appendChars)
    {
        const CharCount AbsoluteMaximumAppendCharLength = 20; // maximum length of uint64 converted to base-10 string

        Assert(maximumAppendCharLength != 0);
        Assert(maximumAppendCharLength <= AbsoluteMaximumAppendCharLength);
        Assert(toString);
        Assert(!toString->IsFinalized());
        Assert(toString->OwnsLastBlock());
        Assert(!(appendChars && !toString->HasOnlyDirectChars()));

        ++maximumAppendCharLength; // + 1 for null terminator
        const CharCount blockCharLength = toString->LastBlockCharLength();
        const bool convertInPlace =
            (appendChars || toString->HasOnlyDirectChars()) &&
            maximumAppendCharLength <= toString->LastBlockCharCapacity() - blockCharLength;
        wchar_t localConvertBuffer[AbsoluteMaximumAppendCharLength + 1]; // + 1 for null terminator
        wchar_t *const convertBuffer = convertInPlace ? &toString->LastBlockChars()[blockCharLength] : localConvertBuffer;
        ConvertToString(value, convertBuffer, maximumAppendCharLength);

        const CharCount appendCharLength = static_cast<CharCount>(wcslen(convertBuffer));
        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"CompoundString::AppendGeneric(TValue &, appendChars = %s) - converted = \"%.8s%s\", appendCharLength = %u\n",
                appendChars ? L"true" : L"false",
                convertBuffer,
                appendCharLength > 8 ? L"..." : L"",
                appendCharLength);
            Output::Flush();
        }

        if(convertInPlace)
        {
            toString->SetLength(toString->GetLength() + appendCharLength);
            toString->SetLastBlockCharLength(blockCharLength + appendCharLength);
            return;
        }
        AnalysisAssert(convertBuffer == localConvertBuffer);
        AppendGeneric(localConvertBuffer, appendCharLength, toString, appendChars);
    }

    template<CharCount AppendCharLengthPlusOne>
    __inline void CompoundString::Append(const wchar_t (&s)[AppendCharLengthPlusOne], const bool isCppLiteral)
    {
        AppendGeneric(s, isCppLiteral, this, false);
    }

    template<CharCount AppendCharLengthPlusOne>
    __inline void CompoundString::AppendChars(const wchar_t (&s)[AppendCharLengthPlusOne], const bool isCppLiteral)
    {
        AppendGeneric(s, isCppLiteral, this, true);
    }

    template<class TValue, class FConvertToString>
    __inline void CompoundString::Append(
        const TValue &value,
        const CharCount maximumAppendCharLength,
        const FConvertToString ConvertToString)
    {
        AppendGeneric(value, maximumAppendCharLength, ConvertToString, this, false);
    }

    template<class TValue, class FConvertToString>
    __inline void CompoundString::AppendChars(
        const TValue &value,
        const CharCount maximumAppendCharLength,
        const FConvertToString ConvertToString)
    {
        AppendGeneric(value, maximumAppendCharLength, ConvertToString, this, true);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #endif
    #pragma endregion
}

//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// JScriptDiag does not link with Runtime.lib and does not include .cpp files, so this file will be included as a header
#pragma once
#include "RuntimeLibraryPch.h"


namespace Js
{
    #pragma region CompoundString::Block
    #ifndef IsJsDiag
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    const uint CompoundString::Block::MaxChainedBlockSize = HeapConstants::MaxSmallObjectSize; // TODO: LargeAlloc seems to be significantly slower, hence this threshold
    const uint CompoundString::Block::ChainSizeThreshold = MaxChainedBlockSize / 2;
    // TODO: Once the above LargeAlloc issue is fixed, experiment with forcing resizing as long as the string has only direct chars

    CompoundString::Block::Block(const CharCount charCapacity, const Block *const previous)
        : bufferOwner(this), charLength(0), charCapacity(charCapacity), previous(previous)
    {
        Assert(HeapInfo::IsAlignedSize(ChainSizeThreshold));
        Assert(ChainSizeThreshold <= MaxChainedBlockSize);
        Assert(HeapInfo::IsAlignedSize(MaxChainedBlockSize));
        Assert((MaxChainedBlockSize << 1) > MaxChainedBlockSize);

        Assert(charCapacity != 0);
        Assert(GrowSize(SizeFromCharCapacity(charCapacity)) != 0);
    }

    CompoundString::Block::Block(
        const void *const buffer,
        const CharCount charLength,
        const CharCount charCapacity)
        : bufferOwner(this), charLength(charLength), charCapacity(charCapacity), previous(nullptr)
    {
        Assert(buffer);
        Assert(charLength <= charCapacity);

        js_wmemcpy_s(Chars(), charLength, Chars(buffer), charLength);
    }

    CompoundString::Block::Block(const Block &other, const CharCount usedCharLength)
        : bufferOwner(other.bufferOwner),
        charLength(usedCharLength),
        charCapacity(other.charCapacity),
        previous(other.previous)
    {
        // This only does a shallow copy. The metadata is copied, and a reference to the other block is included in this copy
        // for access to the other block's buffer.
        Assert(usedCharLength <= other.charCapacity);
    }

    CompoundString::Block *CompoundString::Block::New(
        const uint size,
        const Block *const previous,
        Recycler *const recycler)
    {
        Assert(HeapInfo::IsAlignedSize(size));
        Assert(recycler);

        return RecyclerNewPlus(recycler, size - sizeof(Block), Block, CharCapacityFromSize(size), previous);
    }

    CompoundString::Block *CompoundString::Block::New(
        const void *const buffer,
        const CharCount usedCharLength,
        const bool reserveMoreSpace,
        Recycler *const recycler)
    {
        Assert(buffer);
        Assert(recycler);

        uint size = SizeFromUsedCharLength(usedCharLength);
        if(reserveMoreSpace)
            size = GrowSize(size);
        return RecyclerNewPlus(recycler, size - sizeof(Block), Block, buffer, usedCharLength, CharCapacityFromSize(size));
    }

    CompoundString::Block *CompoundString::Block::Clone(
        const CharCount usedCharLength,
        Recycler *const recycler) const
    {
        Assert(recycler);

        return RecyclerNew(recycler, Block, *this, usedCharLength);
    }

    CharCount CompoundString::Block::CharCapacityFromSize(const uint size)
    {
        Assert(size >= sizeof(Block));

        return (size - sizeof(Block)) / sizeof(wchar_t);
    }

    uint CompoundString::Block::SizeFromCharCapacity(const CharCount charCapacity)
    {
        Assert(IsValidCharCount(charCapacity));
        return UInt32Math::Add(sizeof(Block), charCapacity * sizeof(wchar_t));
    }

    #endif

    inline CharCount CompoundString::Block::PointerAlign(const CharCount charLength)
    {
        const CharCount alignedCharLength = ::Math::Align(charLength, static_cast<CharCount>(sizeof(void *) / sizeof(wchar_t)));
        Assert(alignedCharLength >= charLength);
        return alignedCharLength;
    }

    inline const wchar_t *CompoundString::Block::Chars(const void *const buffer)
    {
        return static_cast<const wchar_t *>(buffer);
    }

    #ifndef IsJsDiag

    wchar_t *CompoundString::Block::Chars(void *const buffer)
    {
        return static_cast<wchar_t *>(buffer);
    }

    #endif

    inline void *const *CompoundString::Block::Pointers(const void *const buffer)
    {
        return static_cast<void *const *>(buffer);
    }

    #ifndef IsJsDiag

    void **CompoundString::Block::Pointers(void *const buffer)
    {
        return static_cast<void **>(buffer);
    }

    CharCount CompoundString::Block::PointerCapacityFromCharCapacity(const CharCount charCapacity)
    {
        return charCapacity / (sizeof(void *) / sizeof(wchar_t));
    }

    CharCount CompoundString::Block::CharCapacityFromPointerCapacity(const CharCount pointerCapacity)
    {
        return pointerCapacity * (sizeof(void *) / sizeof(wchar_t));
    }

    #endif

    inline CharCount CompoundString::Block::PointerLengthFromCharLength(const CharCount charLength)
    {
        return PointerAlign(charLength) / (sizeof(void *) / sizeof(wchar_t));
    }

    #ifndef IsJsDiag

    CharCount CompoundString::Block::CharLengthFromPointerLength(const CharCount pointerLength)
    {
        return pointerLength * (sizeof(void *) / sizeof(wchar_t));
    }

    uint CompoundString::Block::SizeFromUsedCharLength(const CharCount usedCharLength)
    {
        const size_t usedSize = SizeFromCharCapacity(usedCharLength);
        const size_t alignedUsedSize = HeapInfo::GetAlignedSizeNoCheck(usedSize);
        if (alignedUsedSize != (uint)alignedUsedSize)
        {
            Js::Throw::OutOfMemory();
        }
        return (uint)alignedUsedSize;
    }

    bool CompoundString::Block::ShouldAppendChars(
        const CharCount appendCharLength,
        const uint additionalSizeForPointerAppend)
    {
        // Append characters instead of pointers when it would save space. Add some buffer as well, as flattening becomes more
        // expensive after the switch to pointer mode.
        //
        // 'additionalSizeForPointerAppend' should be provided when appending a pointer also involves creating a string object
        // or some other additional space (such as LiteralString, in which case this parameter should be sizeof(LiteralString)),
        // as that additional size also needs to be taken into account.
        return appendCharLength <= (sizeof(void *) * 2 + additionalSizeForPointerAppend) / sizeof(wchar_t);
    }

    const void *CompoundString::Block::Buffer() const
    {
        return bufferOwner + 1;
    }

    void *CompoundString::Block::Buffer()
    {
        return bufferOwner + 1;
    }

    const CompoundString::Block *CompoundString::Block::Previous() const
    {
        return previous;
    }

    const wchar_t *CompoundString::Block::Chars() const
    {
        return Chars(Buffer());
    }

    wchar_t *CompoundString::Block::Chars()
    {
        return Chars(Buffer());
    }

    CharCount CompoundString::Block::CharLength() const
    {
        return charLength;
    }

    void CompoundString::Block::SetCharLength(const CharCount charLength)
    {
        Assert(charLength <= CharCapacity());

        this->charLength = charLength;
    }

    CharCount CompoundString::Block::CharCapacity() const
    {
        return charCapacity;
    }

    void *const *CompoundString::Block::Pointers() const
    {
        return Pointers(Buffer());
    }

    void **CompoundString::Block::Pointers()
    {
        return Pointers(Buffer());
    }

    CharCount CompoundString::Block::PointerLength() const
    {
        return PointerLengthFromCharLength(CharLength());
    }

    CharCount CompoundString::Block::PointerCapacity() const
    {
        return PointerCapacityFromCharCapacity(CharCapacity());
    }

    uint CompoundString::Block::GrowSize(const uint size)
    {
        Assert(size >= sizeof(Block));
        Assert(HeapInfo::IsAlignedSize(size));

        const uint newSize = size << 1;
        Assert(newSize > size);
        return newSize;
    }

    uint CompoundString::Block::GrowSizeForChaining(const uint size)
    {
        const uint newSize = GrowSize(size);
        return min(MaxChainedBlockSize, newSize);
    }

    CompoundString::Block *CompoundString::Block::Chain(Recycler *const recycler)
    {
        return New(GrowSizeForChaining(SizeFromUsedCharLength(CharLength())), this, recycler);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #endif
    #pragma endregion

    #pragma region CompoundString::BlockInfo
    #ifndef IsJsDiag
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    CompoundString::BlockInfo::BlockInfo() : buffer(nullptr), charLength(0), charCapacity(0)
    {
    }

    CompoundString::BlockInfo::BlockInfo(Block *const block)
    {
        CopyFrom(block);
    }

    wchar_t *CompoundString::BlockInfo::Chars() const
    {
        return Block::Chars(buffer);
    }

    CharCount CompoundString::BlockInfo::CharLength() const
    {
        return charLength;
    }

    void CompoundString::BlockInfo::SetCharLength(const CharCount charLength)
    {
        Assert(charLength <= CharCapacity());

        this->charLength = charLength;
    }

    CharCount CompoundString::BlockInfo::CharCapacity() const
    {
        return charCapacity;
    }

    void **CompoundString::BlockInfo::Pointers() const
    {
        return Block::Pointers(buffer);
    }

    CharCount CompoundString::BlockInfo::PointerLength() const
    {
        return Block::PointerLengthFromCharLength(CharLength());
    }

    void CompoundString::BlockInfo::SetPointerLength(const CharCount pointerLength)
    {
        Assert(pointerLength <= PointerCapacity());

        charLength = Block::CharLengthFromPointerLength(pointerLength);
    }

    CharCount CompoundString::BlockInfo::PointerCapacity() const
    {
        return Block::PointerCapacityFromCharCapacity(CharCapacity());
    }

    CharCount CompoundString::BlockInfo::AlignCharCapacityForAllocation(const CharCount charCapacity)
    {
        const CharCount alignedCharCapacity =
            ::Math::AlignOverflowCheck(
                charCapacity == 0 ? static_cast<CharCount>(1) : charCapacity,
                static_cast<CharCount>(HeapConstants::ObjectGranularity / sizeof(wchar_t)));
        Assert(alignedCharCapacity != 0);
        return alignedCharCapacity;
    }

    CharCount CompoundString::BlockInfo::GrowCharCapacity(const CharCount charCapacity)
    {
        Assert(charCapacity != 0);
        Assert(AlignCharCapacityForAllocation(charCapacity) == charCapacity);

        const CharCount newCharCapacity = UInt32Math::Mul<2>(charCapacity);
        Assert(newCharCapacity > charCapacity);
        return newCharCapacity;
    }

    bool CompoundString::BlockInfo::ShouldAllocateBuffer(const CharCount charCapacity)
    {
        Assert(charCapacity != 0);
        Assert(AlignCharCapacityForAllocation(charCapacity) == charCapacity);

        return charCapacity < Block::ChainSizeThreshold / sizeof(wchar_t);
    }

    void CompoundString::BlockInfo::AllocateBuffer(const CharCount charCapacity, Recycler *const recycler)
    {
        Assert(!buffer);
        Assert(CharLength() == 0);
        Assert(CharCapacity() == 0);
        Assert(ShouldAllocateBuffer(charCapacity));
        Assert(recycler);

        buffer = RecyclerNewArray(recycler, wchar_t, charCapacity);
        this->charCapacity = charCapacity;
    }

    CompoundString::Block *CompoundString::BlockInfo::CopyBuffer(
        const void *const buffer,
        const CharCount usedCharLength,
        const bool reserveMoreSpace,
        Recycler *const recycler)
    {
        Assert(buffer);
        Assert(recycler);

        CharCount charCapacity = AlignCharCapacityForAllocation(usedCharLength);
        if(reserveMoreSpace)
            charCapacity = GrowCharCapacity(charCapacity);
        if(ShouldAllocateBuffer(charCapacity))
        {
            AllocateBuffer(charCapacity, recycler);
            charLength = usedCharLength;
            js_wmemcpy_s((wchar_t*)(this->buffer), charCapacity, (wchar_t*)(buffer), usedCharLength);
            return nullptr;
        }

        Block *const block = Block::New(buffer, usedCharLength, reserveMoreSpace, recycler);
        CopyFrom(block);
        return block;
    }

    CompoundString::Block *CompoundString::BlockInfo::Resize(Recycler *const recycler)
    {
        Assert(recycler);

        const CharCount newCharCapacity = GrowCharCapacity(AlignCharCapacityForAllocation(CharLength()));
        if(ShouldAllocateBuffer(newCharCapacity))
        {
            void *const newBuffer = RecyclerNewArray(recycler, wchar_t, newCharCapacity);
            charCapacity = newCharCapacity;
            const CharCount charLength = CharLength();
            js_wmemcpy_s((wchar_t*)newBuffer, charCapacity, (wchar_t*)buffer, charLength);
            buffer = newBuffer;
            return nullptr;
        }

        Block *const block = Block::New(buffer, CharLength(), true, recycler);
        CopyFrom(block);
        return block;
    }

    void CompoundString::BlockInfo::CopyFrom(Block *const block)
    {
        buffer = block->Buffer();
        charLength = block->CharLength();
        charCapacity = block->CharCapacity();
    }

    void CompoundString::BlockInfo::CopyTo(Block *const block)
    {
        Assert(block->Buffer() == buffer);
        Assert(block->CharLength() <= charLength);
        Assert(block->CharCapacity() == charCapacity);

        block->SetCharLength(charLength);
    }

    void CompoundString::BlockInfo::Unreference()
    {
        buffer = nullptr;
        charLength = 0;
        charCapacity = 0;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #endif
    #pragma endregion

    #pragma region CompoundString
    #ifndef IsJsDiag
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    CompoundString::CompoundString(const CharCount initialCharCapacity, JavascriptLibrary *const library)
        : LiteralString(library->GetStringTypeStatic()),
        directCharLength(static_cast<CharCount>(-1)),
        ownsLastBlock(true),
        lastBlock(nullptr)
    {
        Assert(library);

        lastBlockInfo.AllocateBuffer(initialCharCapacity, library->GetRecycler());
    }

    CompoundString::CompoundString(
        const CharCount initialBlockSize,
        const bool allocateBlock,
        JavascriptLibrary *const library)
        : LiteralString(library->GetStringTypeStatic()),
        directCharLength(static_cast<CharCount>(-1)),
        ownsLastBlock(true)
    {
        Assert(allocateBlock);
        Assert(library);

        Block *const block = Block::New(initialBlockSize, nullptr, library->GetRecycler());
        lastBlockInfo.CopyFrom(block);
        lastBlock = block;
    }

    CompoundString::CompoundString(
        const CharCount stringLength,
        const CharCount directCharLength,
        const void *const buffer,
        const CharCount usedCharLength,
        const bool reserveMoreSpace,
        JavascriptLibrary *const library)
        : LiteralString(library->GetStringTypeStatic()),
        directCharLength(directCharLength),
        ownsLastBlock(true)
    {
        Assert(directCharLength == static_cast<CharCount>(-1) || directCharLength <= stringLength);
        Assert(buffer);
        Assert(library);

        SetLength(stringLength);
        lastBlock = lastBlockInfo.CopyBuffer(buffer, usedCharLength, reserveMoreSpace, library->GetRecycler());
    }

    CompoundString::CompoundString(CompoundString &other, const bool forAppending)
        : LiteralString(other.GetLibrary()->GetStringTypeStatic()),
        lastBlockInfo(other.lastBlockInfo),
        directCharLength(other.directCharLength),
        lastBlock(other.lastBlock)
    {
        Assert(!other.IsFinalized());

        SetLength(other.GetLength());

        if(forAppending)
        {
            // This compound string will be used for appending, so take ownership of the last block. Appends are fast for a
            // compound string that owns the last block.
            const bool ownsLastBlock = other.ownsLastBlock;
            other.ownsLastBlock = false;
            this->ownsLastBlock = ownsLastBlock;
            if(ownsLastBlock)
                return;
            TakeOwnershipOfLastBlock();
            return;
        }

        ownsLastBlock = false;
    }

    CompoundString *CompoundString::NewWithCharCapacity(
        const CharCount initialCharCapacity,
        JavascriptLibrary *const library)
    {
        const CharCount alignedInitialCharCapacity = BlockInfo::AlignCharCapacityForAllocation(initialCharCapacity);
        if(BlockInfo::ShouldAllocateBuffer(alignedInitialCharCapacity))
            return NewWithBufferCharCapacity(alignedInitialCharCapacity, library);
        return NewWithBlockSize(Block::SizeFromUsedCharLength(initialCharCapacity), library);
    }

    CompoundString *CompoundString::NewWithPointerCapacity(
        const CharCount initialPointerCapacity,
        JavascriptLibrary *const library)
    {
        return NewWithCharCapacity(Block::CharCapacityFromPointerCapacity(initialPointerCapacity), library);
    }

    CompoundString *CompoundString::NewWithBufferCharCapacity(const CharCount initialCharCapacity, JavascriptLibrary *const library)
    {
        Assert(library);

        return RecyclerNew(library->GetRecycler(), CompoundString, initialCharCapacity, library);
    }

    CompoundString *CompoundString::NewWithBlockSize(const CharCount initialBlockSize, JavascriptLibrary *const library)
    {
        Assert(library);

        return RecyclerNew(library->GetRecycler(), CompoundString, initialBlockSize, true, library);
    }

    CompoundString *CompoundString::New(
        const CharCount stringLength,
        const CharCount directCharLength,
        const void *const buffer,
        const CharCount usedCharLength,
        const bool reserveMoreSpace,
        JavascriptLibrary *const library)
    {
        Assert(library);

        return
            RecyclerNew(
                library->GetRecycler(),
                CompoundString,
                stringLength,
                directCharLength,
                buffer,
                usedCharLength,
                reserveMoreSpace,
                library);
    }

    CompoundString *CompoundString::Clone(const bool forAppending)
    {
        return RecyclerNew(GetLibrary()->GetRecycler(), CompoundString, *this, forAppending);
    }

    CompoundString * CompoundString::JitClone(CompoundString * cs)
    {
        Assert(Is(cs));
        return cs->Clone(false);
    }

    CompoundString * CompoundString::JitCloneForAppending(CompoundString * cs)
    {
        Assert(Is(cs));
        return cs->Clone(true);
    }

    bool CompoundString::Is(RecyclableObject *const object)
    {
        return VirtualTableInfo<CompoundString>::HasVirtualTable(object);
    }

    bool CompoundString::Is(const Var var)
    {
        return RecyclableObject::Is(var) && Is(RecyclableObject::FromVar(var));
    }

    CompoundString *CompoundString::FromVar(RecyclableObject *const object)
    {
        Assert(Is(object));

        CompoundString *const cs = static_cast<CompoundString *>(object);
        Assert(!cs->IsFinalized());
        return cs;
    }

    CompoundString *CompoundString::FromVar(const Var var)
    {
        return FromVar(RecyclableObject::FromVar(var));
    }

    JavascriptString *CompoundString::GetImmutableOrScriptUnreferencedString(JavascriptString *const s)
    {
        Assert(s);

        // The provided string may be referenced by script code. A script-unreferenced version of the string is being requested,
        // likely because the provided string will be referenced directly in a concatenation operation (by ConcatString or
        // another CompoundString, for instance). If the provided string is a CompoundString, it must not be mutated by script
        // code after the concatenation operation. In that case, clone the string to ensure that it is not referenced by script
        // code. If the clone is never handed back to script code, it effectively behaves as an immutable string.
        return Is(s) ? FromVar(s)->Clone(false) : s;
    }

    bool CompoundString::ShouldAppendChars(const CharCount appendCharLength)
    {
        return Block::ShouldAppendChars(appendCharLength);
    }

    bool CompoundString::HasOnlyDirectChars() const
    {
        return directCharLength == static_cast<CharCount>(-1);
    }

    void CompoundString::SwitchToPointerMode()
    {
        Assert(HasOnlyDirectChars());

        directCharLength = GetLength();

        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(L"CompoundString::SwitchToPointerMode()\n");
            Output::Flush();
        }
    }

    bool CompoundString::OwnsLastBlock() const
    {
        return ownsLastBlock;
    }

    const wchar_t *CompoundString::GetAppendStringBuffer(JavascriptString *const s) const
    {
        Assert(s);

        // A compound string cannot flatten itself while appending itself to itself since flattening would make the append
        // illegal. Clone the string being appended if necessary, before flattening.
        return s == this ? FromVar(s)->Clone(false)->GetSz() : s->GetString();
    }

    wchar_t *CompoundString::LastBlockChars() const
    {
        return lastBlockInfo.Chars();
    }

    CharCount CompoundString::LastBlockCharLength() const
    {
        return lastBlockInfo.CharLength();
    }

    void CompoundString::SetLastBlockCharLength(const CharCount charLength)
    {
        lastBlockInfo.SetCharLength(charLength);
    }

    CharCount CompoundString::LastBlockCharCapacity() const
    {
        return lastBlockInfo.CharCapacity();
    }

    void **CompoundString::LastBlockPointers() const
    {
        return lastBlockInfo.Pointers();
    }

    CharCount CompoundString::LastBlockPointerLength() const
    {
        return lastBlockInfo.PointerLength();
    }

    void CompoundString::SetLastBlockPointerLength(const CharCount pointerLength)
    {
        lastBlockInfo.SetPointerLength(pointerLength);
    }

    CharCount CompoundString::LastBlockPointerCapacity() const
    {
        return lastBlockInfo.PointerCapacity();
    }

    void CompoundString::PackSubstringInfo(
        const CharCount startIndex,
        const CharCount length,
        void * *const packedSubstringInfoRef,
        void * *const packedSubstringInfo2Ref)
    {
        Assert(static_cast<int32>(startIndex) >= 0);
        Assert(static_cast<int32>(length) >= 0);
        Assert(packedSubstringInfoRef);
        Assert(packedSubstringInfo2Ref);

    #if defined(_M_X64_OR_ARM64)
        // On 64-bit architectures, two nonnegative 32-bit ints fit completely in a tagged pointer
        *packedSubstringInfoRef =
            reinterpret_cast<void *>(
                (static_cast<uintptr_t>(startIndex) << 32) +
                (static_cast<uintptr_t>(length) << 1) +
                1);
        *packedSubstringInfo2Ref = nullptr;
    #else
        CompileAssert(sizeof(void *) == sizeof(int32));

        // On 32-bit architectures, it will be attempted to fit both pieces of into one pointer by using 16 bits for the
        // start index, 15 for the length, and 1 for the tag. If it does not fit, an additional pointer will be used.
        if(startIndex <= static_cast<CharCount>(0xffff) && length <= static_cast<CharCount>(0x7fff))
        {
            *packedSubstringInfoRef =
                reinterpret_cast<void *>(
                    (static_cast<uintptr_t>(startIndex) << 16) +
                    (static_cast<uintptr_t>(length) << 1) +
                    1);
            *packedSubstringInfo2Ref = nullptr;
        }
        else
        {
            *packedSubstringInfoRef = reinterpret_cast<void *>((static_cast<uintptr_t>(startIndex) << 1) + 1);
            *packedSubstringInfo2Ref = reinterpret_cast<void *>((static_cast<uintptr_t>(length) << 1) + 1);
        }
    #endif

    #if DBG
        CharCount unpackedStartIndex, unpackedLength;
        UnpackSubstringInfo(*packedSubstringInfoRef, *packedSubstringInfo2Ref, &unpackedStartIndex, &unpackedLength);
        Assert(unpackedStartIndex == startIndex);
        Assert(unpackedLength == length);
    #endif
    }

    #endif

    inline bool CompoundString::IsPackedInfo(void *const pointer)
    {
        Assert(pointer);

        return reinterpret_cast<uintptr_t>(pointer) & 1;
    }

    inline void CompoundString::UnpackSubstringInfo(
        void *const pointer,
        void *const pointer2,
        CharCount *const startIndexRef,
        CharCount *const lengthRef)
    {
        Assert(pointer);
        Assert(startIndexRef);
        Assert(lengthRef);

        const uintptr_t packedSubstringInfo = reinterpret_cast<uintptr_t>(pointer);
        Assert(packedSubstringInfo & 1);

    #if defined(_M_X64_OR_ARM64)
        // On 64-bit architectures, two nonnegative 32-bit ints fit completely in a tagged pointer
        Assert(!pointer2);
        *startIndexRef = static_cast<CharCount>(packedSubstringInfo >> 32);
        *lengthRef = static_cast<CharCount>(static_cast<uint32>(packedSubstringInfo) >> 1);
    #else
        CompileAssert(sizeof(void *) == sizeof(int32));

        // On 32-bit architectures, it will be attempted to fit both pieces of into one pointer by using 16 bits for the
        // start index, 15 for the length, and 1 for the tag. If it does not fit, an additional pointer will be used.
        if(!pointer2)
        {
            *startIndexRef = static_cast<CharCount>(packedSubstringInfo >> 16);
            *lengthRef = static_cast<CharCount>(static_cast<uint16>(packedSubstringInfo) >> 1);
        }
        else
        {
            *startIndexRef = static_cast<CharCount>(packedSubstringInfo >> 1);
            const uintptr_t packedSubstringInfo2 = reinterpret_cast<uintptr_t>(pointer2);
            Assert(packedSubstringInfo2 & 1);
            *lengthRef = static_cast<CharCount>(packedSubstringInfo2 >> 1);
        }
    #endif
    }

    #ifndef IsJsDiag

    void CompoundString::AppendSlow(const wchar_t c)
    {
        Grow();
        const bool appended =
            HasOnlyDirectChars()
                ? TryAppendGeneric(c, this)
                : TryAppendGeneric(GetLibrary()->GetCharStringCache().GetStringForChar(c), 1, this);
        Assert(appended);
    }

    void CompoundString::AppendSlow(JavascriptString *const s)
    {
        Grow();
        const bool appended = TryAppendGeneric(s, s->GetLength(), this);
        Assert(appended);
    }

    void CompoundString::AppendSlow(
        __in_xcount(appendCharLength) const wchar_t *const s,
        const CharCount appendCharLength)
    {
        Assert(!IsFinalized());
        Assert(OwnsLastBlock());
        Assert(HasOnlyDirectChars());
        Assert(s);

        // In case of exception, save enough state to revert back to the current state
        const BlockInfo savedLastBlockInfo(lastBlockInfo);
        Block *const savedLastBlock = lastBlock;
        const CharCount savedStringLength = GetLength();

        SetLength(savedStringLength + appendCharLength);

        CharCount copiedCharLength = 0;
        while(true)
        {
            const CharCount blockCharLength = LastBlockCharLength();
            const CharCount copyCharLength =
                min(LastBlockCharCapacity() - blockCharLength, appendCharLength - copiedCharLength);
            CopyHelper(&LastBlockChars()[blockCharLength], &s[copiedCharLength], copyCharLength);
            SetLastBlockCharLength(blockCharLength + copyCharLength);
            copiedCharLength += copyCharLength;
            if(copiedCharLength >= appendCharLength)
                break;
            try
            {
                Grow();
            }
            catch(...)
            {
                lastBlockInfo = savedLastBlockInfo;
                if(savedLastBlock)
                    savedLastBlock->SetCharLength(savedLastBlockInfo.CharLength());
                lastBlock = savedLastBlock;
                SetLength(savedStringLength);
                throw;
            }
        }

        Assert(copiedCharLength == appendCharLength);
    }

    void CompoundString::AppendSlow(
        JavascriptString *const s,
        void *const packedSubstringInfo,
        void *const packedSubstringInfo2,
        const CharCount appendCharLength)
    {
        Grow();
        const bool appended = TryAppendGeneric(s, packedSubstringInfo, packedSubstringInfo2, appendCharLength, this);
        Assert(appended);
    }

    void CompoundString::PrepareForAppend()
    {
        Assert(!IsFinalized());

        if(OwnsLastBlock())
            return;
        TakeOwnershipOfLastBlock();
    }

    void CompoundString::Append(const wchar_t c)
    {
        AppendGeneric(c, this, false);
    }

    void CompoundString::AppendChars(const wchar_t c)
    {
        AppendGeneric(c, this, true);
    }

    void CompoundString::Append(JavascriptString *const s)
    {
        AppendGeneric(s, this, false);
    }

    void CompoundString::AppendChars(JavascriptString *const s)
    {
        AppendGeneric(s, this, true);
    }

    void CompoundString::Append(
        JavascriptString *const s,
        const CharCount startIndex,
        const CharCount appendCharLength)
    {
        AppendGeneric(s, startIndex, appendCharLength, this, false);
    }

    void CompoundString::AppendChars(
        JavascriptString *const s,
        const CharCount startIndex,
        const CharCount appendCharLength)
    {
        AppendGeneric(s, startIndex, appendCharLength, this, true);
    }

    void CompoundString::Append(
        __in_xcount(appendCharLength) const wchar_t *const s,
        const CharCount appendCharLength)
    {
        AppendGeneric(s, appendCharLength, this, false);
    }

    void CompoundString::AppendChars(
        __in_xcount(appendCharLength) const wchar_t *const s,
        const CharCount appendCharLength)
    {
        AppendGeneric(s, appendCharLength, this, true);
    }

    void CompoundString::AppendCharsSz(__in_z const wchar_t *const s)
    {
        size_t len = wcslen(s);
        // We limit the length of the string to MaxCharCount,
        // so just OOM if we are appending a string that exceed this limit already
        if (!IsValidCharCount(len))
        {
            JavascriptExceptionOperators::ThrowOutOfMemory(this->GetScriptContext());
        }
        AppendChars(s, (CharCount)len);
    }

    void CompoundString::Grow()
    {
        Assert(!IsFinalized());
        Assert(OwnsLastBlock());

        Block *const lastBlock = this->lastBlock;
        if(!lastBlock)
        {
            // There is no last block. Only the buffer was allocated, and is held in 'lastBlockInfo'. In that case it is always
            // within the threshold to resize. Resize the buffer or resize it into a new block depending on its size.
            this->lastBlock = lastBlockInfo.Resize(GetLibrary()->GetRecycler());
            return;
        }

        lastBlockInfo.CopyTo(lastBlock);
        Block *const newLastBlock = lastBlock->Chain(GetLibrary()->GetRecycler());
        lastBlockInfo.CopyFrom(newLastBlock);
        this->lastBlock = newLastBlock;
    }

    void CompoundString::TakeOwnershipOfLastBlock()
    {
        Assert(!IsFinalized());
        Assert(!OwnsLastBlock());

        // Another string object owns the last block's buffer. The buffer must be copied, or another block must be chained.

        Block *const lastBlock = this->lastBlock;
        if(!lastBlock)
        {
            // There is no last block. Only the buffer was allocated, and is held in 'lastBlockInfo'. In that case it is always
            // within the threshold to resize. Resize the buffer or resize it into a new block depending on its size.
            this->lastBlock = lastBlockInfo.Resize(GetLibrary()->GetRecycler());
            ownsLastBlock = true;
            return;
        }

        // The last block is already in a chain, or is over the threshold to resize. Shallow-clone the last block (clone
        // just its metadata, while still pointing to the original buffer), and chain it to a new last block.
        Recycler *const recycler = GetLibrary()->GetRecycler();
        Block *const newLastBlock = lastBlock->Clone(LastBlockCharLength(), recycler)->Chain(recycler);
        lastBlockInfo.CopyFrom(newLastBlock);
        ownsLastBlock = true;
        this->lastBlock = newLastBlock;
    }

    void CompoundString::Unreference()
    {
        lastBlockInfo.Unreference();
        directCharLength = 0;
        ownsLastBlock = false;
        lastBlock = nullptr;
    }

    const wchar_t *CompoundString::GetSz()
    {
        Assert(!IsFinalized());

        const CharCount totalCharLength = GetLength();
        switch(totalCharLength)
        {
            case 0:
            {
                Unreference();
                const wchar_t *const buffer = L"";
                SetBuffer(buffer);
                VirtualTableInfo<LiteralString>::SetVirtualTable(this);
                return buffer;
            }

            case 1:
            {
                Assert(HasOnlyDirectChars());
                Assert(LastBlockCharLength() == 1);

                const wchar_t *const buffer = GetLibrary()->GetCharStringCache().GetStringForChar(LastBlockChars()[0])->UnsafeGetBuffer();
                Unreference();
                SetBuffer(buffer);
                VirtualTableInfo<LiteralString>::SetVirtualTable(this);
                return buffer;
            }
        }

        if(OwnsLastBlock() && HasOnlyDirectChars() && !lastBlock && TryAppendGeneric(L'\0', this)) // GetSz() requires null termination
        {
            // There is no last block. Only the buffer was allocated, and is held in 'lastBlockInfo'. Since this string owns the
            // last block, has only direct chars, and the buffer was allocated directly (buffer pointer is not an internal
            // pointer), there is no need to copy the buffer.
            SetLength(totalCharLength); // terminating null should not count towards the string length
            const wchar_t *const buffer = LastBlockChars();
            Unreference();
            SetBuffer(buffer);
            VirtualTableInfo<LiteralString>::SetVirtualTable(this);
            return buffer;
        }

        wchar_t *const buffer = RecyclerNewArrayLeaf(GetScriptContext()->GetRecycler(), wchar_t, SafeSzSize(totalCharLength));
        buffer[totalCharLength] = L'\0'; // GetSz() requires null termination
        Copy<CompoundString>(buffer, totalCharLength);
        Assert(buffer[totalCharLength] == L'\0');
        Unreference();
        SetBuffer(buffer);
        VirtualTableInfo<LiteralString>::SetVirtualTable(this);
        return buffer;
    }

    void CompoundString::CopyVirtual(
        _Out_writes_(m_charLength) wchar_t *const buffer,
        StringCopyInfoStack &nestedStringTreeCopyInfos,
        const byte recursionDepth)
    {
        Assert(!IsFinalized());
        Assert(buffer);

        const CharCount totalCharLength = GetLength();
        switch(totalCharLength)
        {
            case 0:
                return;

            case 1:
                Assert(HasOnlyDirectChars());
                Assert(LastBlockCharLength() == 1);

                buffer[0] = LastBlockChars()[0];
                return;
        }

        // Copy buffers from string pointers
        const bool hasOnlyDirectChars = HasOnlyDirectChars();
        const CharCount directCharLength = hasOnlyDirectChars ? totalCharLength : this->directCharLength;
        CharCount remainingCharLengthToCopy = totalCharLength;
        const Block *const lastBlock = this->lastBlock;
        const Block *block = lastBlock;
        void *const *blockPointers = LastBlockPointers();
        CharCount pointerIndex = LastBlockPointerLength();
        while(remainingCharLengthToCopy > directCharLength)
        {
            while(pointerIndex == 0)
            {
                Assert(block);
                block = block->Previous();
                Assert(block);
                blockPointers = block->Pointers();
                pointerIndex = block->PointerLength();
            }

            void *const pointer = blockPointers[--pointerIndex];
            if(IsPackedInfo(pointer))
            {
                Assert(pointerIndex != 0);
                void *pointer2 = blockPointers[--pointerIndex];
                JavascriptString *s;
    #if defined(_M_X64_OR_ARM64)
                Assert(!IsPackedInfo(pointer2));
    #else
                if(IsPackedInfo(pointer2))
                {
                    Assert(pointerIndex != 0);
                    s = JavascriptString::FromVar(blockPointers[--pointerIndex]);
                }
                else
    #endif
                {
                    s = JavascriptString::FromVar(pointer2);
                    pointer2 = nullptr;
                }

                CharCount startIndex, copyCharLength;
                UnpackSubstringInfo(pointer, pointer2, &startIndex, &copyCharLength);
                Assert(startIndex <= s->GetLength());
                Assert(copyCharLength <= s->GetLength() - startIndex);

                Assert(remainingCharLengthToCopy >= copyCharLength);
                remainingCharLengthToCopy -= copyCharLength;
                CopyHelper(&buffer[remainingCharLengthToCopy], &s->GetString()[startIndex], copyCharLength);
            }
            else
            {
                JavascriptString *const s = JavascriptString::FromVar(pointer);
                const CharCount copyCharLength = s->GetLength();

                Assert(remainingCharLengthToCopy >= copyCharLength);
                remainingCharLengthToCopy -= copyCharLength;
                if(recursionDepth == MaxCopyRecursionDepth && s->IsTree())
                {
                    // Don't copy nested string trees yet, as that involves a recursive call, and the recursion can become
                    // excessive. Just collect the nested string trees and the buffer location where they should be copied, and
                    // the caller can deal with those after returning.
                    nestedStringTreeCopyInfos.Push(StringCopyInfo(s, &buffer[remainingCharLengthToCopy]));
                }
                else
                {
                    Assert(recursionDepth <= MaxCopyRecursionDepth);
                    s->Copy(&buffer[remainingCharLengthToCopy], nestedStringTreeCopyInfos, recursionDepth + 1);
                }
            }
        }

        Assert(remainingCharLengthToCopy == directCharLength);
        if(remainingCharLengthToCopy != 0)
        {
            // Determine the number of direct chars in the current block
            CharCount blockCharLength;
            if(pointerIndex == 0)
            {
                // The string switched to pointer mode at the beginning of the current block, or the string never switched to
                // pointer mode and the last block is empty. In either case, direct chars span to the end of the previous block.
                Assert(block);
                block = block->Previous();
                Assert(block);
                blockCharLength = block->CharLength();
            }
            else if(hasOnlyDirectChars)
            {
                // The string never switched to pointer mode, so the current block's char length is where direct chars end
                blockCharLength = block == lastBlock ? LastBlockCharLength() : block->CharLength();
            }
            else
            {
                // The string switched to pointer mode somewhere in the middle of the current block. To determine where direct
                // chars end in this block, all previous blocks are scanned and their char lengths discounted.
                blockCharLength = remainingCharLengthToCopy;
                if(block)
                {
                    for(const Block *previousBlock = block->Previous();
                        previousBlock;
                        previousBlock = previousBlock->Previous())
                    {
                        Assert(blockCharLength >= previousBlock->CharLength());
                        blockCharLength -= previousBlock->CharLength();
                    }
                }
                Assert(Block::PointerLengthFromCharLength(blockCharLength) == pointerIndex);
            }

            // Copy direct chars
            const wchar_t *blockChars = block == lastBlock ? LastBlockChars() : block->Chars();
            while(true)
            {
                if(blockCharLength != 0)
                {
                    Assert(remainingCharLengthToCopy >= blockCharLength);
                    remainingCharLengthToCopy -= blockCharLength;
                    js_wmemcpy_s(&buffer[remainingCharLengthToCopy], blockCharLength, blockChars, blockCharLength);
                    if(remainingCharLengthToCopy == 0)
                        break;
                }

                Assert(block);
                block = block->Previous();
                Assert(block);
                blockChars = block->Chars();
                blockCharLength = block->CharLength();
            }
        }

    #if DBG
        // Verify that all nonempty blocks have been visited
        if(block)
        {
            while(true)
            {
                block = block->Previous();
                if(!block)
                    break;
                Assert(block->CharLength() == 0);
            }
        }
    #endif

        Assert(remainingCharLengthToCopy == 0);
    }

    bool CompoundString::IsTree() const
    {
        Assert(!IsFinalized());

        return !HasOnlyDirectChars();
    }

    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(CompoundString);

    CompileAssert(static_cast<CharCount>(-1) > static_cast<CharCount>(0)); // CharCount is assumed to be unsigned

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #endif
    #pragma endregion
}

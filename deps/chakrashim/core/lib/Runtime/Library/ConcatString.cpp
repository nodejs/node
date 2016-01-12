//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"



namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(ConcatString);

    // Note: see also: ConcatString.inl

    /////////////////////// ConcatStringBase //////////////////////////

    ConcatStringBase::ConcatStringBase(StaticType* stringType) : LiteralString(stringType)
    {
    }

    // Copy the content of items into specified buffer.
    void ConcatStringBase::CopyImpl(_Out_writes_(m_charLength) wchar_t *const buffer,
            int itemCount, _In_reads_(itemCount) JavascriptString * const * items,
            StringCopyInfoStack &nestedStringTreeCopyInfos, const byte recursionDepth)
    {

        Assert(!IsFinalized());
        Assert(buffer);

        CharCount copiedCharLength = 0;
        for(int i = 0; i < itemCount; ++i)
        {
            JavascriptString *const s = items[i];
            if(!s)
            {
                continue;
            }

            if (s->IsFinalized())
            {
                // If we have the buffer already, just copy it
                const CharCount copyCharLength = s->GetLength();
                Assert(copiedCharLength + copyCharLength <= this->GetLength());
                CopyHelper(&buffer[copiedCharLength], s->GetString(), copyCharLength);
                copiedCharLength += copyCharLength;
                continue;
            }

            if(i == itemCount - 1)
            {
                JavascriptString * const * newItems;
                int newItemCount = s->GetRandomAccessItemsFromConcatString(newItems);
                if (newItemCount != -1)
                {
                    // Optimize for right-weighted ConcatString tree (the append case). Even though appending to a ConcatString will
                    // transition into a CompoundString fairly quickly, strings created by doing just a few appends are very common.
                    items = newItems;
                    itemCount = newItemCount;
                    i = -1;
                    continue;
                }
            }

            const CharCount copyCharLength = s->GetLength();
            Assert(copyCharLength <= GetLength() - copiedCharLength);

            if(recursionDepth == MaxCopyRecursionDepth && s->IsTree())
            {
                // Don't copy nested string trees yet, as that involves a recursive call, and the recursion can become
                // excessive. Just collect the nested string trees and the buffer location where they should be copied, and
                // the caller can deal with those after returning.
                nestedStringTreeCopyInfos.Push(StringCopyInfo(s, &buffer[copiedCharLength]));
            }
            else
            {
                Assert(recursionDepth <= MaxCopyRecursionDepth);
                s->Copy(&buffer[copiedCharLength], nestedStringTreeCopyInfos, recursionDepth + 1);
            }
            copiedCharLength += copyCharLength;
        }

        Assert(copiedCharLength == GetLength());
    }

    bool ConcatStringBase::IsTree() const
    {
        Assert(!IsFinalized());

        return true;
    }

    /////////////////////// ConcatString //////////////////////////

    ConcatString::ConcatString(JavascriptString* a, JavascriptString* b) :
        ConcatStringN<2>(a->GetLibrary()->GetStringTypeStatic(), false)
    {
        Assert(a);
        Assert(b);

        a = CompoundString::GetImmutableOrScriptUnreferencedString(a);
        b = CompoundString::GetImmutableOrScriptUnreferencedString(b);

        m_slots[0] = a;
        m_slots[1] = b;

        this->SetLength(a->GetLength() + b->GetLength()); // does not include null character
    }

    ConcatString* ConcatString::New(JavascriptString* left, JavascriptString* right)
    {
        Assert(left);

#ifdef PROFILE_STRINGS
       StringProfiler::RecordConcatenation( left->GetScriptContext(), left->GetLength(), right->GetLength(), ConcatType_ConcatTree);
#endif
        Recycler* recycler = left->GetScriptContext()->GetRecycler();
        return RecyclerNew(recycler, ConcatString, left, right);
    }

    /////////////////////// ConcatStringBuilder //////////////////////////

    ConcatStringBuilder::ConcatStringBuilder(ScriptContext* scriptContext, int initialSlotCount) :
        ConcatStringBase(scriptContext->GetLibrary()->GetStringTypeStatic()),
        m_count(0), m_prevChunk(NULL)
    {
        Assert(scriptContext);

        // Note: m_slotCount is a valid scenario -- when you don't know how many will be there.
        this->AllocateSlots(initialSlotCount);
        this->SetLength(0); // does not include null character
    }

    ConcatStringBuilder::ConcatStringBuilder(const ConcatStringBuilder& other):
        ConcatStringBase(other.GetScriptContext()->GetLibrary()->GetStringTypeStatic())
    {
        m_slots = other.m_slots;
        m_count = other.m_count;
        m_slotCount = other.m_slotCount;
        m_prevChunk = other.m_prevChunk;
        this->SetLength(other.GetLength());
        // TODO: should we copy the JavascriptString buffer and if so, how do we pass over the ownership?
    }

    ConcatStringBuilder* ConcatStringBuilder::New(ScriptContext* scriptContext, int initialSlotCount)
    {
        Assert(scriptContext);

        return RecyclerNew(scriptContext->GetRecycler(), ConcatStringBuilder, scriptContext, initialSlotCount);
    }

    const wchar_t * ConcatStringBuilder::GetSz()
    {
        const wchar_t * sz = GetSzImpl<ConcatStringBuilder>();

        // Allow a/b to be garbage collected if no more refs.
        ConcatStringBuilder* current = this;
        while (current != NULL)
        {
            memset(current->m_slots, 0, current->m_count * sizeof(JavascriptString*));
            current = current->m_prevChunk;
        }

        return sz;
    }

    // Append/concat a new string to us.
    // The idea is that we will grow/realloc current slot if new size fits into MAX chunk size (c_maxChunkSlotCount).
    // Otherwise we will create a new chunk.
    void ConcatStringBuilder::Append(JavascriptString* str)
    {
        // Note: we are quite lucky here because we always add 1 (no more) string to us.

        Assert(str);
        charcount_t len = this->GetLength(); // This is len of all chunks.

        if (m_count == m_slotCount)
        {
            // Out of free slots, current chunk is full, need to grow.
            int oldItemCount = this->GetItemCount();
            int newItemCount = oldItemCount > 0 ?
                oldItemCount > 1 ? oldItemCount + oldItemCount / 2 : 2 :
                1;
            Assert(newItemCount > oldItemCount);
            int growDelta = newItemCount - oldItemCount; // # of items to grow by.
            int newSlotCount = m_slotCount + growDelta;
            if (newSlotCount <= c_maxChunkSlotCount)
            {
                // While we fit into MAX chunk size, realloc/grow current chunk.
                JavascriptString** newSlots = RecyclerNewArray(this->GetScriptContext()->GetRecycler(), JavascriptString*, newSlotCount);
                memcpy_s(newSlots, newSlotCount * sizeof(JavascriptString*), m_slots, m_slotCount * sizeof(JavascriptString*));
                m_slots = newSlots;
                m_slotCount = newSlotCount;
            }
            else
            {
                // Create new chunk with MAX size, swap new instance's data with this's data.
                // We never create more than one chunk at a time.
                ConcatStringBuilder* newChunk = RecyclerNew(this->GetScriptContext()->GetRecycler(), ConcatStringBuilder, *this); // Create a copy.
                m_prevChunk = newChunk;
                m_count = 0;
                AllocateSlots(this->c_maxChunkSlotCount);
                Assert(m_slots);
            }
        }

        str = CompoundString::GetImmutableOrScriptUnreferencedString(str);

        m_slots[m_count++] = str;

        len += str->GetLength();
        this->SetLength(len);
    }

    // Allocate slots, set m_slots and m_slotCount.
    // Note: the amount of slots allocated can be less than the requestedSlotCount parameter.
    void ConcatStringBuilder::AllocateSlots(int requestedSlotCount)
    {
        if (requestedSlotCount > 0)
        {
            m_slotCount = min(requestedSlotCount, this->c_maxChunkSlotCount);
            m_slots = RecyclerNewArray(this->GetScriptContext()->GetRecycler(), JavascriptString*, m_slotCount);
        }
        else
        {
            m_slotCount = 0;
            m_slots = 0;
        }
    }

    // Returns the number of JavascriptString* items accumulated so far in all chunks.
    int ConcatStringBuilder::GetItemCount() const
    {
        int count = 0;
        const ConcatStringBuilder* current = this;
        while (current != NULL)
        {
            count += current->m_count;
            current = current->m_prevChunk;
        }
        return count;
    }

    ConcatStringBuilder* ConcatStringBuilder::GetHead() const
    {
        ConcatStringBuilder* current = const_cast<ConcatStringBuilder*>(this);
        ConcatStringBuilder* head;
        do
        {
            head = current;
            current = current->m_prevChunk;
        } while (current != NULL);
        return head;
    }

    void ConcatStringBuilder::CopyVirtual(
        _Out_writes_(m_charLength) wchar_t *const buffer,
        StringCopyInfoStack &nestedStringTreeCopyInfos,
        const byte recursionDepth)
    {
        Assert(!this->IsFinalized());
        Assert(buffer);

        CharCount remainingCharLengthToCopy = GetLength();
        for(const ConcatStringBuilder *current = this; current; current = current->m_prevChunk)
        {
            for(int i = current->m_count - 1; i >= 0; --i)
            {
                JavascriptString *const s = current->m_slots[i];
                if(!s)
                {
                    continue;
                }

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
    }

    /////////////////////// ConcatStringMulti //////////////////////////
    ConcatStringMulti::ConcatStringMulti(uint slotCount, JavascriptString * a1, JavascriptString * a2, StaticType* stringTypeStatic) :
        ConcatStringBase(stringTypeStatic), slotCount(slotCount)
    {
#if DBG
        memset(m_slots, 0, slotCount * sizeof(JavascriptString* ));
#endif
        m_slots[0] = CompoundString::GetImmutableOrScriptUnreferencedString(a1);
        m_slots[1] = CompoundString::GetImmutableOrScriptUnreferencedString(a2);

        this->SetLength(a1->GetLength() + a2->GetLength());
    }

    size_t
    ConcatStringMulti::GetAllocSize(uint slotCount)
    {
        return sizeof(ConcatStringMulti) + (sizeof(JavascriptString *) * slotCount);
    }

    ConcatStringMulti*
    ConcatStringMulti::New(uint slotCount, JavascriptString * a1, JavascriptString * a2, ScriptContext * scriptContext)
    {
        return RecyclerNewPlus(scriptContext->GetRecycler(),
            sizeof(JavascriptString *) * slotCount, ConcatStringMulti, slotCount, a1, a2,
            scriptContext->GetLibrary()->GetStringTypeStatic());
    }

    bool
    ConcatStringMulti::Is(Var var)
    {
        return VirtualTableInfo<ConcatStringMulti>::HasVirtualTable(var);
    }

    ConcatStringMulti *
    ConcatStringMulti::FromVar(Var var)
    {
        Assert(ConcatStringMulti::Is(var));
        return static_cast<ConcatStringMulti *>(var);
    }

    const wchar_t *
    ConcatStringMulti::GetSz()
    {
        Assert(IsFilled());
        const wchar_t * sz = GetSzImpl<ConcatStringMulti>();

        // Allow slots to be garbage collected if no more refs.
        memset(m_slots, 0, slotCount * sizeof(JavascriptString*));

        return sz;
    }

    void
    ConcatStringMulti::SetItem(_In_range_(0, slotCount - 1) uint index, JavascriptString* value)
    {
        Assert(index < slotCount);
        Assert(m_slots[index] == nullptr);
        value = CompoundString::GetImmutableOrScriptUnreferencedString(value);
        this->SetLength(this->GetLength() + value->GetLength());
        m_slots[index] = value;
    }

#if DBG
    bool
    ConcatStringMulti::IsFilled() const
    {
        for (uint i = slotCount; i > 0; i--)
        {
            if (m_slots[i - 1] == nullptr) { return false; }
        }
        return true;
    }
#endif
} // namespace Js.

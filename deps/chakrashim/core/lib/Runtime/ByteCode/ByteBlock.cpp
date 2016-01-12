//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

namespace Js
{
    uint ByteBlock::GetLength() const
    {
        return m_contentSize;
    }

    const byte* ByteBlock::GetBuffer() const
    {
        return m_content;
    }

    byte* ByteBlock::GetBuffer()
    {
        return m_content;
    }

    const byte ByteBlock::operator[](uint itemIndex) const
    {
        AssertMsg(itemIndex < m_contentSize, "Ensure valid offset");

        return m_content[itemIndex];
    }

    byte& ByteBlock::operator[] (uint itemIndex)
    {
        AssertMsg(itemIndex < m_contentSize, "Ensure valid offset");

        return m_content[itemIndex];
    }

    ByteBlock *ByteBlock::New(Recycler *alloc, const byte * initialContent, int initialContentSize)
    {
        // initialContent may be 'null' if no data to copy
        AssertMsg(initialContentSize > 0, "Must have valid data size");

        ByteBlock *newBlock = RecyclerNew(alloc, ByteBlock, initialContentSize, alloc);

        //
        // Copy any optional data into the block:
        // - If initialContent was not provided, the block's contents will be uninitialized.
        //
        if (initialContent != nullptr)
        {
            js_memcpy_s(newBlock->m_content, newBlock->GetLength(), initialContent, initialContentSize);
        }

        return newBlock;
    }

    ByteBlock *ByteBlock::NewFromArena(ArenaAllocator *alloc, const byte * initialContent, int initialContentSize)
    {
        // initialContent may be 'null' if no data to copy
        AssertMsg(initialContentSize > 0, "Must have valid data size");

        ByteBlock *newBlock = Anew(alloc, ByteBlock, initialContentSize, alloc);

        //
        // Copy any optional data into the block:
        // - If initialContent was not provided, the block's contents will be uninitialized.
        //
        if (initialContent != nullptr)
        {
            js_memcpy_s(newBlock->m_content, newBlock->GetLength(), initialContent, initialContentSize);
        }

        return newBlock;
    }

    ByteBlock * ByteBlock::Clone(Recycler* alloc)
    {
        return ByteBlock::New(alloc, this->m_content, this->m_contentSize);
    }

    ByteBlock *ByteBlock::New(Recycler *alloc, const byte * initialContent, int initialContentSize, ScriptContext * requestContext)
    {
        // initialContent may be 'null' if no data to copy
        AssertMsg(initialContentSize > 0, "Must have valid data size");

        ByteBlock *newBlock = RecyclerNew(alloc, ByteBlock, initialContentSize, alloc);

        //
        // Copy any optional data into the block:
        // - If initialContent was not provided, the block's contents will be uninitialized.
        //
        if (initialContent != nullptr)
        {
            //
            // Treat initialContent as array of vars
            // Clone vars to the requestContext
            //

            Var *src = (Var*)initialContent;
            Var *dst = (Var*)newBlock->m_content;
            size_t count = initialContentSize / sizeof(Var);

            for (size_t i = 0; i < count; i++)
            {
                if (TaggedInt::Is(src[i]))
                {
                    dst[i] = src[i];
                }
                else
                {
                    //
                    // Currently only numbers are put into AuxiliaryContext data
                    //
                    Assert(JavascriptNumber::Is(src[i]));
                    dst[i] = JavascriptNumber::CloneToScriptContext(src[i], requestContext);
                    requestContext->BindReference(dst[i]);
                }
            }
        }

        return newBlock;
    }

    //
    // Create a copy of buffer
    // Each Var is cloned on the requestContext
    //
    ByteBlock * ByteBlock::Clone(Recycler* alloc, ScriptContext * requestContext)
    {
        return ByteBlock::New(alloc, this->m_content, this->m_contentSize, requestContext);
    }
}

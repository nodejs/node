//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    enum TypeId;
    class DetachedStateBase
    {
    protected:
        TypeId typeId;
        bool hasBeenClaimed;

    public:
        DetachedStateBase(TypeId typeId)
            : typeId(typeId),
            hasBeenClaimed(false)
        {
        }

        TypeId GetTypeId() { return typeId; }

        bool HasBeenClaimed() { return hasBeenClaimed; }

        void MarkAsClaimed() { hasBeenClaimed = true; }

        void CleanUp()
        {
            if (!hasBeenClaimed)
            {
                DiscardState();
            }
            ClearSelfOnly();
        }

        virtual void ClearSelfOnly() = 0;
        virtual void DiscardState() = 0;
        virtual void Discard() = 0;
    };

    typedef enum ArrayBufferAllocationType
    {
        Heap = 0x0,
        CoTask = 0x1,
        MemAlloc = 0x02
    } ArrayBufferAllocationType;

    class ArrayBufferDetachedStateBase : public DetachedStateBase
    {
    public:
        BYTE* buffer;
        uint32 bufferLength;
        ArrayBufferAllocationType allocationType;

        ArrayBufferDetachedStateBase(TypeId typeId, BYTE* buffer, uint32 bufferLength, ArrayBufferAllocationType allocationType)
            : DetachedStateBase(typeId),
            buffer(buffer),
            bufferLength(bufferLength),
            allocationType(allocationType)
        {}

    };
}

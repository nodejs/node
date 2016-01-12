//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    enum CallFlags : unsigned
    {
        CallFlags_None    = 0,
        CallFlags_New     = 1,
        CallFlags_Value   = 2,
        CallFlags_Eval    = 4,
        CallFlags_ExtraArg = 8,
        CallFlags_NotUsed = 0x10,
        CallFlags_Wrapped = 0x20,
        CallFlags_NewTarget = 0x40,
        CallFlags_InternalFrame = 0x80
    };

    struct CallInfo
    {
        CallInfo() {}

        /*
         * Removed the copy constructor because it forced the 64 bit compiler
         * to pass this object by reference. Interpreter stack setup code expects
         * CallInfo to be passed by value.
         */
        CallInfo(ushort count)
            : Flags(CallFlags_None)
            , Count(count)
#ifdef _WIN64
            , unused(0)
#endif
        {
        }

        CallInfo(CallFlags flags, ushort count)
            : Flags(flags)
            , Count(count)
#ifdef _WIN64
            , unused(0)
#endif
        {
        }

        CallInfo(VirtualTableInfoCtorEnum v)
        {
        }

        // Assumes big-endian layout
        // If the size of the count is changed, change should happen at following places also
        //  - scriptdirect.idl
        //  - LowererMDArch::LoadInputParamCount
        //
        unsigned  Count : 24;
        CallFlags Flags : 8;
#ifdef _WIN64
        unsigned unused : 32;
#endif

    public:
        static const ushort ksizeofCount;
        static const ushort ksizeofCallFlags;
        static const uint kMaxCountArgs;
    };

    struct InlineeCallInfo
    {
        // Assumes big-endian layout.
        size_t Count: 4;
        size_t InlineeStartOffset: sizeof(void*) * CHAR_BIT - 4;
        static size_t const MaxInlineeArgoutCount = 0xF;

        static bool Encode(Js::Var &callInfo, size_t count, size_t offset)
        {
            const size_t offsetMask = (~(size_t)0) >> 4;
            const size_t countMask  = 0x0000000F;
            if (count != (count & countMask))
            {
                return false;
            }

            if (offset != (offset & offsetMask))
            {
                return false;
            }

            callInfo = (Js::Var)((offset << 4) | count);

            return true;
        }

        void Clear()
        {
            this->Count = 0;
            this->InlineeStartOffset = 0;
        }
    };
}

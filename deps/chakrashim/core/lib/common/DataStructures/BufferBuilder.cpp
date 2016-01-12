//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"

#include "Option.h"
#include "ImmutableList.h"
#include "BufferBuilder.h"

namespace Js
{

#if DBG
void
BufferBuilder::TraceOutput(byte * buffer, uint32 size) const
{
    if (PHASE_TRACE1(Js::ByteCodeSerializationPhase))
    {
        Output::Print(L"%08X: %-40s:", this->offset, this->clue);
        for (uint i = 0; i < size; i ++)
        {
            Output::Print(L" %02x", buffer[this->offset + i]);
        }
        Output::Print(L"\n");
    }
}
#endif

};

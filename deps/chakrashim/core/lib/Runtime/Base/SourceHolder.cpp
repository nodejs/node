//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

namespace Js
{
    LPCUTF8 const ISourceHolder::emptyString = (LPCUTF8)"\0";
    SimpleSourceHolder const ISourceHolder::emptySourceHolder(emptyString, 0, true);

    ISourceHolder* SimpleSourceHolder::Clone(ScriptContext* scriptContext)
    {
        if(this == ISourceHolder::GetEmptySourceHolder())
        {
            return this;
        }

        utf8char_t * newUtf8String = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), utf8char_t, byteLength + 1);
        js_memcpy_s(newUtf8String, byteLength + 1, this->source, byteLength + 1);
        return RecyclerNew(scriptContext->GetRecycler(), SimpleSourceHolder, newUtf8String, byteLength);
    }
}

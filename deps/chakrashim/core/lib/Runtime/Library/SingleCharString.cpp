//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(SingleCharString);

    SingleCharString::SingleCharString(wchar_t ch, StaticType * type) : JavascriptString(type, 1, m_buff)
    {
        m_buff[0] = ch;
        m_buff[1] = L'\0';

#ifdef PROFILE_STRINGS
        StringProfiler::RecordNewString( this->GetScriptContext(), this->m_buff, 1 );
#endif
    }

    /*static*/ SingleCharString* SingleCharString::New(wchar_t ch, ScriptContext* scriptContext)
    {
        Assert(scriptContext != nullptr);

        return RecyclerNew(scriptContext->GetRecycler(),SingleCharString,ch,
            scriptContext->GetLibrary()->GetStringTypeStatic());
    }

    /*static*/ SingleCharString* SingleCharString::New(wchar_t ch, ScriptContext* scriptContext, ArenaAllocator* arena)
    {
        Assert(scriptContext != nullptr);
        Assert(arena != nullptr);

        return Anew(arena, SingleCharString, ch,
            scriptContext->GetLibrary()->GetStringTypeStatic());
    }
}

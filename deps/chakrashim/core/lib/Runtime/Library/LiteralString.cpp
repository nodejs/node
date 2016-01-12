//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(LiteralString);

    LiteralString::LiteralString(StaticType * type) : JavascriptString(type)
    {
    }

    LiteralString::LiteralString(StaticType * type, const wchar_t* content, charcount_t charLength) :
        JavascriptString(type, charLength, content)
    {
#if defined(DBG) && defined(_M_IX86)
        // Make sure content isn't on the stack by comparing to stack bounds in TIB
        AssertMsg(!ThreadContext::IsOnStack((void*)content),
            "LiteralString object created using stack buffer...");
#endif
       AssertMsg(AutoSystemInfo::IsJscriptModulePointer((void *)content)
           || type->GetScriptContext()->GetRecycler()->IsValidObject((void *)content),
           "LiteralString can only be used with static or GC strings");

#ifdef PROFILE_STRINGS
        StringProfiler::RecordNewString( type->GetScriptContext(), content, charLength );
#endif
    }

    LiteralString* LiteralString::New(StaticType* type, const wchar_t* content, charcount_t charLength, Recycler* recycler)
    {
        return RecyclerNew(recycler, LiteralString, type, content, charLength);
    }

    LiteralString* LiteralString::CreateEmptyString(StaticType* type)
    {
        return RecyclerNew(type->GetScriptContext()->GetRecycler(), LiteralString, type, L"", 0);
    }


    ArenaLiteralString::ArenaLiteralString(StaticType * type, const wchar_t* content, charcount_t charLength) :
      JavascriptString(type, charLength, content)
    {
#if defined(DBG) && defined(_M_IX86)
        // Make sure content isn't on the stack by comparing to stack bounds in TIB
        AssertMsg(!ThreadContext::IsOnStack((void*)content),
            "ArenaLiteralString object created using stack buffer...");
#endif

        AssertMsg(!type->GetScriptContext()->GetRecycler()->IsValidObject((void *)content),
            "ArenaLiteralString should not be used with GC strings");

#ifdef PROFILE_STRINGS
        StringProfiler::RecordNewString( type->GetScriptContext(), content, charLength );
#endif
    }

    ArenaLiteralString* ArenaLiteralString::New(StaticType* type, const wchar_t* content, charcount_t charLength, Recycler* recycler)
    {
        return RecyclerNew(recycler, ArenaLiteralString, type, content, charLength);
    }

    ArenaLiteralString* ArenaLiteralString::New(StaticType* type, const wchar_t* content, charcount_t charLength, ArenaAllocator* arena)
    {
        return Anew(arena, ArenaLiteralString, type, content, charLength);
    }

    RecyclableObject * ArenaLiteralString::CloneToScriptContext(ScriptContext* requestContext)
    {
        return JavascriptString::NewCopyBuffer(this->GetSz(), this->GetLength(), requestContext);
    }
} // namespace Js

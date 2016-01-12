//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(SubString);

    __inline SubString::SubString(void const * originalFullStringReference, const wchar_t* subString, charcount_t length, ScriptContext *scriptContext) :
        JavascriptString(scriptContext->GetLibrary()->GetStringTypeStatic())
    {
        this->SetBuffer(subString);
        this->originalFullStringReference = originalFullStringReference;
        this->SetLength(length);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordNewString( scriptContext, this->UnsafeGetBuffer(), this->GetLength() );
#endif
    }

    JavascriptString* SubString::New(JavascriptString* string, charcount_t start, charcount_t length)
    {
        AssertMsg( IsValidCharCount(start), "start is out of range" );
        AssertMsg( IsValidCharCount(length), "length is out of range" );

        ScriptContext *scriptContext = string->GetScriptContext();
        if (!length)
        {
            return scriptContext->GetLibrary()->GetEmptyString();
        }

        Recycler* recycler = scriptContext->GetRecycler();

        Assert(string->GetLength() >= start + length);
        const wchar_t * subString = string->GetString() + start;
        void const * originalFullStringReference = string->GetOriginalStringReference();

        return RecyclerNew(recycler, SubString, originalFullStringReference, subString, length, scriptContext);
    }

    JavascriptString* SubString::New(const wchar_t* string, charcount_t start, charcount_t length, ScriptContext *scriptContext)
    {
        AssertMsg( IsValidCharCount(start), "start is out of range" );
        AssertMsg( IsValidCharCount(length), "length is out of range" );

        if (!length)
        {
            return scriptContext->GetLibrary()->GetEmptyString();
        }

        Recycler* recycler = scriptContext->GetRecycler();
        return RecyclerNew(recycler, SubString, string, string + start, length, scriptContext);
    }

    const wchar_t* SubString::GetSz()
    {
        if (originalFullStringReference)
        {
            Recycler* recycler = this->GetScriptContext()->GetRecycler();
            wchar_t * newInstance = AllocateLeafAndCopySz(recycler, UnsafeGetBuffer(), GetLength());
            this->SetBuffer(newInstance);

            // We don't need the string reference anymore, set it to nullptr and use this to know our string is nullptr terminated
            originalFullStringReference = nullptr;
        }

        return UnsafeGetBuffer();
    }

    const void * SubString::GetOriginalStringReference()
    {
        if (originalFullStringReference != nullptr)
        {
            return originalFullStringReference;
        }
        return __super::GetOriginalStringReference();
    }

    size_t SubString::GetAllocatedByteCount() const
    {
        if (originalFullStringReference)
        {
            return 0;
        }
        return __super::GetAllocatedByteCount();
    }

    bool SubString::IsSubstring() const
    {
        if (originalFullStringReference)
        {
            return true;
        }
        return false;
    }

}

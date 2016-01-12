//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    template <>
    Var JavascriptTypedNumber<__int64>::ToVar(__int64 value, ScriptContext* scriptContext)
    {
        if (!TaggedInt::IsOverflow(value))
        {
            return TaggedInt::ToVarUnchecked((int)value);
        }
        JavascriptTypedNumber<__int64>* number = RecyclerNewLeaf(scriptContext->GetRecycler(), JavascriptInt64Number, value,
            scriptContext->GetLibrary()->GetInt64TypeStatic());
        return number;
    }

    template <>
    Var JavascriptTypedNumber<unsigned __int64>::ToVar(unsigned __int64 value, ScriptContext* scriptContext)
    {
        if (!TaggedInt::IsOverflow(value))
        {
            return TaggedInt::ToVarUnchecked((uint)value);
        }
        JavascriptTypedNumber<unsigned __int64>* number = RecyclerNewLeaf(scriptContext->GetRecycler(), JavascriptUInt64Number, value,
            scriptContext->GetLibrary()->GetUInt64TypeStatic());
        return number;
    }

    template <>
    JavascriptString* JavascriptTypedNumber<__int64>::ToString(Var value, ScriptContext* scriptContext)
    {
        wchar_t szBuffer[30];
        __int64 val = JavascriptTypedNumber<__int64>::FromVar(value)->GetValue();
        errno_t err = _i64tow_s(val, szBuffer, 30, 10);
        AssertMsg(err == 0, "convert int64 to string failed");
        return JavascriptString::NewCopySz(szBuffer, scriptContext);
    }

    template <>
    JavascriptString* JavascriptTypedNumber<unsigned __int64>::ToString(Var value, ScriptContext* scriptContext)
    {
        wchar_t szBuffer[30];
        unsigned __int64 val = JavascriptUInt64Number::FromVar(value)->GetValue();
        errno_t err = _ui64tow_s(val, szBuffer, 30, 10);
        AssertMsg(err == 0, "convert int64 to string failed");
        return JavascriptString::NewCopySz(szBuffer, scriptContext);
    }

    template <typename T>
    RecyclableObject* JavascriptTypedNumber<T>::ToObject(ScriptContext * requestContext)
    {
        return requestContext->GetLibrary()->CreateNumberObjectWithCheck((double)m_value);
    }
}

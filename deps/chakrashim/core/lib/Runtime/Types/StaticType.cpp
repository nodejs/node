//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    StaticType *
    StaticType::New(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint)
    {
        return RecyclerNew(scriptContext->GetRecycler(), StaticType, scriptContext, typeId, prototype, entryPoint);
    }

    bool
    StaticType::Is(TypeId typeId)
    {
        return typeId <= TypeIds_LastStaticType;
    }

    BOOL RecyclableObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        ENTER_PINNED_SCOPE(JavascriptString, valueStr);
        ScriptContext *scriptContext = GetScriptContext();

        switch(GetTypeId())
        {
        case TypeIds_Undefined:
            valueStr = GetLibrary()->GetUndefinedDisplayString();
            break;
        case TypeIds_Null:
            valueStr = GetLibrary()->GetNullDisplayString();
            break;
        case TypeIds_Integer:
            valueStr = scriptContext->GetIntegerString(this);
            break;
        case TypeIds_Boolean:
            valueStr = JavascriptBoolean::FromVar(this)->GetValue() ?
                           GetLibrary()->GetTrueDisplayString()
                         : GetLibrary()->GetFalseDisplayString();
            break;
        case TypeIds_Number:
            valueStr = JavascriptNumber::ToStringRadix10(JavascriptNumber::GetValue(this), scriptContext);
            break;
        case TypeIds_String:
            valueStr = JavascriptString::FromVar(this);
            break;
        default:
            valueStr = GetLibrary()->GetUndefinedDisplayString();
        }

        stringBuilder->Append(valueStr->GetString(), valueStr->GetLength());

        LEAVE_PINNED_SCOPE();

        return TRUE;
    }

    BOOL RecyclableObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        switch(GetTypeId())
        {
        case TypeIds_Undefined:
            stringBuilder->AppendCppLiteral(L"Undefined");
            break;
        case TypeIds_Null:
            stringBuilder->AppendCppLiteral(L"Null");
            break;
        case TypeIds_Integer:
        case TypeIds_Number:
            stringBuilder->AppendCppLiteral(L"Number");
            break;
        case TypeIds_Boolean:
            stringBuilder->AppendCppLiteral(L"Boolean");
            break;
        case TypeIds_String:
            stringBuilder->AppendCppLiteral(L"String");
            break;
        default:
            stringBuilder->AppendCppLiteral(L"Object, (Static Type)");
            break;
        }

        return TRUE;
    }
}

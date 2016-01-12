//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptBooleanObject::JavascriptBooleanObject(JavascriptBoolean* value, DynamicType * type)
        : DynamicObject(type), value(value)
    {
        Assert(type->GetTypeId() == TypeIds_BooleanObject);
    }

    bool JavascriptBooleanObject::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_BooleanObject;
    }

    JavascriptBooleanObject* JavascriptBooleanObject::FromVar(Js::Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptBooleanObject'");

        return static_cast<JavascriptBooleanObject *>(RecyclableObject::FromVar(aValue));
    }

    BOOL JavascriptBooleanObject::GetValue() const
    {
        if (this->value == nullptr)
        {
            return false;
        }
        return this->value->GetValue();
    }

    void JavascriptBooleanObject::Initialize(JavascriptBoolean* value)
    {
        Assert(this->value == nullptr);

        this->value = value;
    }

    BOOL JavascriptBooleanObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        if (this->GetValue())
        {
            JavascriptString* trueDisplayString = GetLibrary()->GetTrueDisplayString();
            stringBuilder->Append(trueDisplayString->GetString(), trueDisplayString->GetLength());
        }
        else
        {
            JavascriptString* falseDisplayString = GetLibrary()->GetFalseDisplayString();
            stringBuilder->Append(falseDisplayString->GetString(), falseDisplayString->GetLength());
        }
        return TRUE;
    }

    BOOL JavascriptBooleanObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"Boolean, (Object)");
        return TRUE;
    }
} // namespace Js

//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    bool JavascriptVariantDate::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_VariantDate;
    }

    JavascriptVariantDate* JavascriptVariantDate::FromVar(Js::Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptVariantDate'");

        return static_cast<JavascriptVariantDate *>(RecyclableObject::FromVar(aValue));
    }

    Var JavascriptVariantDate::GetTypeOfString(ScriptContext* requestContext)
    {
        return requestContext->GetLibrary()->CreateStringFromCppLiteral(L"date");
    }

    JavascriptString* JavascriptVariantDate::GetValueString(ScriptContext* scriptContext)
    {
        return DateImplementation::ConvertVariantDateToString(this->value, scriptContext);
    }

    BOOL JavascriptVariantDate::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        BOOL ret;

        ENTER_PINNED_SCOPE(JavascriptString, resultString);
        resultString = DateImplementation::ConvertVariantDateToString(this->value, GetScriptContext());
        if (resultString != nullptr)
        {
            stringBuilder->Append(resultString->GetString(), resultString->GetLength());
            ret = TRUE;
        }
        else
        {
            ret = FALSE;
        }

        LEAVE_PINNED_SCOPE();

        return ret;
    }

    BOOL JavascriptVariantDate::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"Date"); // For whatever reason in IE8 jscript, typeof returns "date"
                                                  // while the debugger displays "Date" for the type
        return TRUE;
    }

    RecyclableObject * JavascriptVariantDate::CloneToScriptContext(ScriptContext* requestContext)
    {
        return requestContext->GetLibrary()->CreateVariantDate(value);
    }

    RecyclableObject* JavascriptVariantDate::ToObject(ScriptContext* requestContext)
    {
        // WOOB 1124298: Just return a new object when converting to object.
        return requestContext->GetLibrary()->CreateObject(true);
    }

    BOOL JavascriptVariantDate::GetProperty(Js::Var originalInstance, Js::PropertyId propertyId, Js::Var* value, PropertyValueInfo* info, Js::ScriptContext* requestContext)
    {
        if (requestContext->GetThreadContext()->RecordImplicitException())
        {
            JavascriptError::ThrowTypeError(requestContext, JSERR_Property_VarDate, requestContext->GetPropertyName(propertyId)->GetBuffer());
        }
        *value = nullptr;
        return true;
    };

    BOOL JavascriptVariantDate::GetProperty(Js::Var originalInstance, Js::JavascriptString* propertyNameString, Js::Var* value, PropertyValueInfo* info, Js::ScriptContext* requestContext)
    {
        if (requestContext->GetThreadContext()->RecordImplicitException())
        {
            JavascriptError::ThrowTypeError(requestContext, JSERR_Property_VarDate, propertyNameString);
        }
        *value = nullptr;
        return true;
    };

    BOOL JavascriptVariantDate::GetPropertyReference(Js::Var originalInstance, Js::PropertyId propertyId, Js::Var* value, PropertyValueInfo* info, Js::ScriptContext* requestContext)
    {
        if (requestContext->GetThreadContext()->RecordImplicitException())
        {
            JavascriptError::ThrowTypeError(requestContext, JSERR_Property_VarDate, requestContext->GetPropertyName(propertyId)->GetBuffer());
        }
        *value = nullptr;
        return true;
    };

    BOOL JavascriptVariantDate::SetProperty(Js::PropertyId propertyId, Js::Var value, Js::PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, scriptContext->GetPropertyName(propertyId)->GetBuffer());
    };

    BOOL JavascriptVariantDate::SetProperty(Js::JavascriptString* propertyNameString, Js::Var value, Js::PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, propertyNameString->GetSz());
    };

    BOOL JavascriptVariantDate::InitProperty(Js::PropertyId propertyId, Js::Var value, PropertyOperationFlags flags, Js::PropertyValueInfo* info)
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, scriptContext->GetPropertyName(propertyId)->GetBuffer());
    };

    BOOL JavascriptVariantDate::DeleteProperty(Js::PropertyId propertyId, Js::PropertyOperationFlags flags)
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, scriptContext->GetPropertyName(propertyId)->GetBuffer());
    };

    BOOL JavascriptVariantDate::GetItemReference(Js::Var originalInstance, uint32 index, Js::Var* value, Js::ScriptContext * scriptContext)
    {
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, JavascriptNumber::ToStringRadix10(index, scriptContext)->GetSz());
    };

    BOOL JavascriptVariantDate::GetItem(Js::Var originalInstance, uint32 index, Js::Var* value, Js::ScriptContext * scriptContext)
    {
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, JavascriptNumber::ToStringRadix10(index, scriptContext)->GetSz());
    };

    BOOL JavascriptVariantDate::SetItem(uint32 index, Js::Var value, Js::PropertyOperationFlags flags)
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, JavascriptNumber::ToStringRadix10(index, scriptContext)->GetSz());
    };

    BOOL JavascriptVariantDate::ToPrimitive(JavascriptHint hint, Var* result, ScriptContext * requestContext)
    {
        if (hint == JavascriptHint::HintString)
        {
            JavascriptString* resultString = this->GetValueString(requestContext);
            if (resultString != nullptr)
            {
                (*result) = resultString;
                return TRUE;
            }
            Assert(false);
        }
        else if (hint == JavascriptHint::HintNumber)
        {
            *result = JavascriptNumber::ToVarNoCheck(DateImplementation::JsUtcTimeFromVarDate(value, requestContext), requestContext);
            return TRUE;
        }
        else
        {
            Assert(hint == JavascriptHint::None);
            *result = this;
            return TRUE;
        }
        return FALSE;
    }

    BOOL JavascriptVariantDate::Equals(Var other, BOOL *value, ScriptContext * requestContext)
    {
        // Calling .Equals on a VT_DATE variant at least gives the "[property name] is null or not An object error"
        *value = FALSE;
        return TRUE;
    }
}

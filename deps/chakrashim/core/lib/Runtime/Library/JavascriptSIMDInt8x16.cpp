//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptSIMDInt8x16::JavascriptSIMDInt8x16(SIMDValue *val, StaticType *type) : RecyclableObject(type), value(*val)
    {
        Assert(type->GetTypeId() == TypeIds_SIMDInt8x16);
    }

    JavascriptSIMDInt8x16* JavascriptSIMDInt8x16::New(SIMDValue *val, ScriptContext* requestContext)
    {
        return (JavascriptSIMDInt8x16 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDInt8x16, val, requestContext->GetLibrary()->GetSIMDInt8x16TypeStatic());
    }

    bool  JavascriptSIMDInt8x16::Is(Var instance)
    {
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDInt8x16;
    }

    JavascriptSIMDInt8x16* JavascriptSIMDInt8x16::FromVar(Var aValue)
    {
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDInt8x16'");

        return reinterpret_cast<JavascriptSIMDInt8x16 *>(aValue);
    }

    JavascriptSIMDInt8x16* JavascriptSIMDInt8x16::FromFloat32x4Bits(JavascriptSIMDFloat32x4   *instance, ScriptContext* requestContext)
    {
        return JavascriptSIMDInt8x16::New(&instance->GetValue(), requestContext);
    }

    JavascriptSIMDInt8x16* JavascriptSIMDInt8x16::FromInt32x4Bits(JavascriptSIMDInt32x4   *instance, ScriptContext* requestContext)
    {
        return JavascriptSIMDInt8x16::New(&instance->GetValue(), requestContext);
    }

    BOOL JavascriptSIMDInt8x16::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return GetPropertyBuiltIns(propertyId, value, requestContext);
    }

    BOOL JavascriptSIMDInt8x16::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, requestContext))
        {
            return true;
        }
        return false;
    }

    BOOL JavascriptSIMDInt8x16::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return JavascriptSIMDInt8x16::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    bool JavascriptSIMDInt8x16::GetPropertyBuiltIns(PropertyId propertyId, Var* value, ScriptContext* requestContext)
    {
        if (propertyId == PropertyIds::toString)
        {
            *value = requestContext->GetLibrary()->GetSIMDInt8x16ToStringFunction();
            return true;
        }
        return false;
    }

    Var JavascriptSIMDInt8x16::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));
        if (args.Info.Count == 0 || JavascriptOperators::GetTypeId(args[0]) != TypeIds_SIMDInt8x16)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedSimd, L"SIMDInt8x16.toString");
        }

        JavascriptSIMDInt8x16* instance = JavascriptSIMDInt8x16::FromVar(args[0]);
        Assert(instance);

        wchar_t stringBuffer[1024];
        SIMDValue value = instance->GetValue();

        swprintf_s(stringBuffer, 1024, L"Int8x16(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)", value.i8[0], value.i8[1], value.i8[2], value.i8[3], value.i8[4], value.i8[5], value.i8[6], value.i8[7], value.i8[8], value.i8[9], value.i8[10], value.i8[11], value.i8[12], value.i8[13], value.i8[14], value.i8[15]);

        JavascriptString* string = JavascriptString::NewCopySzFromArena(stringBuffer, scriptContext, scriptContext->GeneralAllocator());

        return string;
    }

    Var JavascriptSIMDInt8x16::Copy(ScriptContext* requestContext)
    {
        return JavascriptSIMDInt8x16::New(&this->value, requestContext);
    }
}

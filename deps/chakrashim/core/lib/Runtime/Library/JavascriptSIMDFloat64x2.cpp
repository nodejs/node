//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptSIMDFloat64x2::JavascriptSIMDFloat64x2(SIMDValue *val, StaticType *type) : RecyclableObject(type), value(*val)
    {
        Assert(type->GetTypeId() == TypeIds_SIMDFloat64x2);
    }

    JavascriptSIMDFloat64x2* JavascriptSIMDFloat64x2::New(SIMDValue *val, ScriptContext* requestContext)
    {
        return (JavascriptSIMDFloat64x2 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDFloat64x2, val, requestContext->GetLibrary()->GetSIMDFloat64x2TypeStatic());
    }

    bool  JavascriptSIMDFloat64x2::Is(Var instance)
    {
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDFloat64x2;
    }

    JavascriptSIMDFloat64x2* JavascriptSIMDFloat64x2::FromVar(Var aValue)
    {
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDFloat64x2'");

        return reinterpret_cast<JavascriptSIMDFloat64x2 *>(aValue);
    }

    JavascriptSIMDFloat64x2* JavascriptSIMDFloat64x2::FromFloat32x4(JavascriptSIMDFloat32x4 *instance, ScriptContext* requestContext)
    {
        SIMDValue result = SIMDFloat64x2Operation::OpFromFloat32x4(instance->GetValue());
        return JavascriptSIMDFloat64x2::New(&result, requestContext);
    }

    JavascriptSIMDFloat64x2* JavascriptSIMDFloat64x2::FromFloat32x4Bits(JavascriptSIMDFloat32x4 *instance, ScriptContext* requestContext)
    {
        return JavascriptSIMDFloat64x2::New(&instance->GetValue(), requestContext);
    }

    JavascriptSIMDFloat64x2* JavascriptSIMDFloat64x2::FromInt32x4(JavascriptSIMDInt32x4   *instance, ScriptContext* requestContext)
    {
        SIMDValue result = SIMDFloat64x2Operation::OpFromInt32x4(instance->GetValue());
        return JavascriptSIMDFloat64x2::New(&result, requestContext);
    }

    JavascriptSIMDFloat64x2* JavascriptSIMDFloat64x2::FromInt32x4Bits(JavascriptSIMDInt32x4   *instance, ScriptContext* requestContext)
    {
        return JavascriptSIMDFloat64x2::New(&instance->GetValue(), requestContext);
    }

    BOOL JavascriptSIMDFloat64x2::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return GetPropertyBuiltIns(propertyId, value, requestContext);
    }

    BOOL JavascriptSIMDFloat64x2::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, requestContext))
        {
            return true;
        }
        return false;
    }

    BOOL JavascriptSIMDFloat64x2::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return JavascriptSIMDFloat64x2::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    bool JavascriptSIMDFloat64x2::GetPropertyBuiltIns(PropertyId propertyId, Var* value, ScriptContext* requestContext)
    {
        switch (propertyId)
        {
        case PropertyIds::toString:
            *value = requestContext->GetLibrary()->GetSIMDFloat64x2ToStringFunction();
            return true;
        case PropertyIds::signMask:
            *value = GetSignMask();
            return true;
        }

        return false;
    }

    // Entry Points

    Var JavascriptSIMDFloat64x2::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || JavascriptOperators::GetTypeId(args[0]) != TypeIds_SIMDFloat64x2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedSimd, L"SIMDFloat64x2.toString");
        }

        JavascriptSIMDFloat64x2 *instance = JavascriptSIMDFloat64x2::FromVar(args[0]);
        Assert(instance);

        wchar_t stringBuffer[1024];
        SIMDValue value = instance->GetValue();

        swprintf_s(stringBuffer, 1024, L"Float64x2(%.1f,%.1f)", value.f64[SIMD_X], value.f64[SIMD_Y]);

        JavascriptString* string = JavascriptString::NewCopySzFromArena(stringBuffer, scriptContext, scriptContext->GeneralAllocator());

        return string;
    }

    // End Entry Points

    Var JavascriptSIMDFloat64x2::Copy(ScriptContext* requestContext)
    {
        return JavascriptSIMDFloat64x2::New(&this->value, requestContext);
    }

    __inline Var JavascriptSIMDFloat64x2::GetSignMask()
    {
        int signMask = SIMDFloat64x2Operation::OpGetSignMask(value);

        return TaggedInt::ToVarUnchecked(signMask);
    }
}

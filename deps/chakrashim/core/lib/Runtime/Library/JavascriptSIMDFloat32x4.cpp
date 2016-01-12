//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptSIMDFloat32x4::JavascriptSIMDFloat32x4(StaticType *type) : RecyclableObject(type)
    {
        Assert(type->GetTypeId() == TypeIds_SIMDFloat32x4);
    }

    JavascriptSIMDFloat32x4::JavascriptSIMDFloat32x4(SIMDValue *val, StaticType *type) : RecyclableObject(type), value(*val)
    {
        Assert(type->GetTypeId() == TypeIds_SIMDFloat32x4);
    }

    JavascriptSIMDFloat32x4* JavascriptSIMDFloat32x4::AllocUninitialized(ScriptContext* requestContext)
    {
        return (JavascriptSIMDFloat32x4 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDFloat32x4, requestContext->GetLibrary()->GetSIMDFloat32x4TypeStatic());
    }

    JavascriptSIMDFloat32x4* JavascriptSIMDFloat32x4::New(SIMDValue *val, ScriptContext* requestContext)
    {
        return (JavascriptSIMDFloat32x4 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDFloat32x4, val, requestContext->GetLibrary()->GetSIMDFloat32x4TypeStatic());
    }

    bool  JavascriptSIMDFloat32x4::Is(Var instance)
    {
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDFloat32x4;
    }

    JavascriptSIMDFloat32x4* JavascriptSIMDFloat32x4::FromVar(Var aValue)
    {
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDFloat32x4'");

        return reinterpret_cast<JavascriptSIMDFloat32x4 *>(aValue);
    }

    JavascriptSIMDFloat32x4* JavascriptSIMDFloat32x4::FromFloat64x2(JavascriptSIMDFloat64x2 *instance, ScriptContext* requestContext)
    {
        SIMDValue result = SIMDFloat32x4Operation::OpFromFloat64x2(instance->GetValue());

        return JavascriptSIMDFloat32x4::New(&result, requestContext);
    }

    JavascriptSIMDFloat32x4* JavascriptSIMDFloat32x4::FromFloat64x2Bits(JavascriptSIMDFloat64x2 *instance, ScriptContext* requestContext)
    {
        return JavascriptSIMDFloat32x4::New(&instance->GetValue(), requestContext);
    }

    JavascriptSIMDFloat32x4* JavascriptSIMDFloat32x4::FromInt32x4(JavascriptSIMDInt32x4 *instance, ScriptContext* requestContext)
    {
        SIMDValue result = SIMDFloat32x4Operation::OpFromInt32x4(instance->GetValue());
        return JavascriptSIMDFloat32x4::New(&result, requestContext);
    }

    JavascriptSIMDFloat32x4* JavascriptSIMDFloat32x4::FromInt32x4Bits(JavascriptSIMDInt32x4 *instance, ScriptContext* requestContext)
    {
        return JavascriptSIMDFloat32x4::New(&instance->GetValue(), requestContext);
    }

    BOOL JavascriptSIMDFloat32x4::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return GetPropertyBuiltIns(propertyId, value, requestContext);
    }

    BOOL JavascriptSIMDFloat32x4::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, requestContext))
        {
            return true;
        }
        return false;
    }

    BOOL JavascriptSIMDFloat32x4::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return JavascriptSIMDFloat32x4::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    bool JavascriptSIMDFloat32x4::GetPropertyBuiltIns(PropertyId propertyId, Var* value, ScriptContext* requestContext)
    {
        switch (propertyId)
        {
        case PropertyIds::toString:
            *value = requestContext->GetLibrary()->GetSIMDFloat32x4ToStringFunction();
            return true;
        case PropertyIds::signMask:
            *value = GetSignMask();
            return true;
        }

        return false;
    }

    // Entry Points

    Var JavascriptSIMDFloat32x4::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || JavascriptOperators::GetTypeId(args[0]) != TypeIds_SIMDFloat32x4)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedSimd, L"SIMDFloat32x4.toString");
        }

        JavascriptSIMDFloat32x4 *instance = JavascriptSIMDFloat32x4::FromVar(args[0]);
        Assert(instance);

        wchar_t stringBuffer[1024];
        SIMDValue value = instance->GetValue();

        swprintf_s(stringBuffer, 1024, L"Float32x4(%.1f,%.1f,%.1f,%.1f)", value.f32[SIMD_X], value.f32[SIMD_Y], value.f32[SIMD_Z], value.f32[SIMD_W]);

        JavascriptString* string = JavascriptString::NewCopySzFromArena(stringBuffer, scriptContext, scriptContext->GeneralAllocator());

        return string;
    }

    // End Entry Points

    Var JavascriptSIMDFloat32x4::Copy(ScriptContext* requestContext)
    {
        return JavascriptSIMDFloat32x4::New(&this->value, requestContext);
    }

    __inline Var JavascriptSIMDFloat32x4::GetSignMask()
    {
        int signMask = SIMDFloat32x4Operation::OpGetSignMask(value);
        return TaggedInt::ToVarUnchecked(signMask);
    }
}

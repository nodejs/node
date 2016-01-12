//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptSIMDInt32x4::JavascriptSIMDInt32x4(StaticType *type) : RecyclableObject(type)
    {
        Assert(type->GetTypeId() == TypeIds_SIMDInt32x4);
    }

    JavascriptSIMDInt32x4::JavascriptSIMDInt32x4(SIMDValue *val, StaticType *type) : RecyclableObject(type), value(*val)
    {
        Assert(type->GetTypeId() == TypeIds_SIMDInt32x4);
    }

    JavascriptSIMDInt32x4* JavascriptSIMDInt32x4::AllocUninitialized(ScriptContext* requestContext)
    {
        return (JavascriptSIMDInt32x4 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDInt32x4, requestContext->GetLibrary()->GetSIMDInt32x4TypeStatic());
    }

    JavascriptSIMDInt32x4* JavascriptSIMDInt32x4::New(SIMDValue *val, ScriptContext* requestContext)
    {
        return (JavascriptSIMDInt32x4 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDInt32x4, val, requestContext->GetLibrary()->GetSIMDInt32x4TypeStatic());
    }

    bool  JavascriptSIMDInt32x4::Is(Var instance)
    {
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDInt32x4;
    }

    JavascriptSIMDInt32x4* JavascriptSIMDInt32x4::FromVar(Var aValue)
    {
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDInt32x4'");

        return reinterpret_cast<JavascriptSIMDInt32x4 *>(aValue);
    }

    JavascriptSIMDInt32x4* JavascriptSIMDInt32x4::FromBool(SIMDValue *val, ScriptContext* requestContext)
    {
        SIMDValue result = SIMDInt32x4Operation::OpFromBool(*val);
        return JavascriptSIMDInt32x4::New(&result, requestContext);
    }

    JavascriptSIMDInt32x4* JavascriptSIMDInt32x4::FromFloat64x2(JavascriptSIMDFloat64x2 *instance, ScriptContext* requestContext)
    {
        SIMDValue result = SIMDInt32x4Operation::OpFromFloat64x2(instance->GetValue());
        return JavascriptSIMDInt32x4::New(&result, requestContext);
    }

    JavascriptSIMDInt32x4* JavascriptSIMDInt32x4::FromFloat64x2Bits(JavascriptSIMDFloat64x2 *instance, ScriptContext* requestContext)
    {
        return JavascriptSIMDInt32x4::New(&instance->GetValue(), requestContext);
    }

    JavascriptSIMDInt32x4* JavascriptSIMDInt32x4::FromFloat32x4(JavascriptSIMDFloat32x4   *instance, ScriptContext* requestContext)
    {
        SIMDValue result = SIMDInt32x4Operation::OpFromFloat32x4(instance->GetValue());
        return JavascriptSIMDInt32x4::New(&result, requestContext);
    }

    JavascriptSIMDInt32x4* JavascriptSIMDInt32x4::FromFloat32x4Bits(JavascriptSIMDFloat32x4   *instance, ScriptContext* requestContext)
    {
        return JavascriptSIMDInt32x4::New(&instance->GetValue(), requestContext);
    }

    BOOL JavascriptSIMDInt32x4::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return GetPropertyBuiltIns(propertyId, value, requestContext);

    }

    BOOL JavascriptSIMDInt32x4::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, requestContext))
        {
            return true;
        }
        return false;
    }

    BOOL JavascriptSIMDInt32x4::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return JavascriptSIMDInt32x4::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    bool JavascriptSIMDInt32x4::GetPropertyBuiltIns(PropertyId propertyId, Var* value, ScriptContext* requestContext)
    {
        switch (propertyId)
        {
        case PropertyIds::toString:
            *value = requestContext->GetLibrary()->GetSIMDInt32x4ToStringFunction();
            return true;

        case PropertyIds::signMask:
            *value = GetSignMask();
            return true;

        case PropertyIds::flagX:
        case PropertyIds::flagY:
        case PropertyIds::flagZ:
        case PropertyIds::flagW:
            *value = GetLaneAsFlag(propertyId - PropertyIds::flagX, requestContext);
            return true;
        }

        return false;
    }

    Var JavascriptSIMDInt32x4::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || JavascriptOperators::GetTypeId(args[0]) != TypeIds_SIMDInt32x4)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedSimd, L"SIMDInt32x4.toString");
        }

        JavascriptSIMDInt32x4* instance = JavascriptSIMDInt32x4::FromVar(args[0]);
        Assert(instance);

        wchar_t stringBuffer[1024];
        SIMDValue value = instance->GetValue();

        swprintf_s(stringBuffer, 1024, L"Int32x4(%d,%d,%d,%d)", value.i32[SIMD_X], value.i32[SIMD_Y], value.i32[SIMD_Z], value.i32[SIMD_W]);

        JavascriptString* string = JavascriptString::NewCopySzFromArena(stringBuffer, scriptContext, scriptContext->GeneralAllocator());

        return string;
    }

    Var JavascriptSIMDInt32x4::Copy(ScriptContext* requestContext)
    {
        return JavascriptSIMDInt32x4::New(&this->value, requestContext);
    }

    Var  JavascriptSIMDInt32x4::CopyAndSetLaneFlag(uint index, BOOL value, ScriptContext* requestContext)
    {
        AnalysisAssert(index < 4);
        Var instance = Copy(requestContext);
        JavascriptSIMDInt32x4 *insValue = JavascriptSIMDInt32x4::FromVar(instance);
        Assert(insValue);
        insValue->value.i32[index] = value ? -1 : 0;
        return instance;
    }

    __inline Var  JavascriptSIMDInt32x4::GetLaneAsFlag(uint index, ScriptContext* requestContext)
    {
        // convert value.i32[index] to TaggedInt
        AnalysisAssert(index < 4);
        return JavascriptNumber::ToVar(value.i32[index] != 0x0, requestContext);
    }

    __inline Var JavascriptSIMDInt32x4::GetSignMask()
    {
        int signMask = SIMDInt32x4Operation::OpGetSignMask(value);

        return TaggedInt::ToVarUnchecked(signMask);
    }
}

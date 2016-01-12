//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#if ENABLE_NATIVE_CODEGEN
namespace Js
{
    // Q: Are we allowed to call this as a constructor ?
    Var SIMDInt32x4Lib::EntryInt32x4(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));    //comment out due to -ls -stress run

        Var undefinedVar = scriptContext->GetLibrary()->GetUndefined();

        int intSIMDX = JavascriptConversion::ToInt32(args.Info.Count >= 2 ? args[1] : undefinedVar, scriptContext);
        int intSIMDY = JavascriptConversion::ToInt32(args.Info.Count >= 3 ? args[2] : undefinedVar, scriptContext);
        int intSIMDZ = JavascriptConversion::ToInt32(args.Info.Count >= 4 ? args[3] : undefinedVar, scriptContext);
        int intSIMDW = JavascriptConversion::ToInt32(args.Info.Count >= 5 ? args[4] : undefinedVar, scriptContext);

        SIMDValue lanes = SIMDInt32x4Operation::OpInt32x4(intSIMDX, intSIMDY, intSIMDZ, intSIMDW);
        return JavascriptSIMDInt32x4::New(&lanes, scriptContext);
    }

    Var SIMDInt32x4Lib::EntryCheck(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));    //comment out due to -ls -stress run

        if (args.Info.Count >= 2 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            return args[1];
        }
        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"int32x4");
    }

    Var SIMDInt32x4Lib::EntryZero(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        SIMDValue lanes = SIMDInt32x4Operation::OpZero();

        return JavascriptSIMDInt32x4::New(&lanes, scriptContext);
    }

    Var SIMDInt32x4Lib::EntrySplat(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        Var undefinedVar = scriptContext->GetLibrary()->GetUndefined();
        int value = JavascriptConversion::ToInt32(args.Info.Count >= 2 ? args[1] : undefinedVar, scriptContext);

        SIMDValue lanes = SIMDInt32x4Operation::OpSplat(value);

        return JavascriptSIMDInt32x4::New(&lanes, scriptContext);
    }

    Var SIMDInt32x4Lib::EntryBool(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // if incoming parameter is undefined, treat it as false (0), otherwise use typical Javascript boolean conversion.
        // after boolean conversion: if it's true, make it to be -1, if it's false, make it to be 0.
        int intSIMD_X = args.Info.Count >= 2 ? (JavascriptConversion::ToBoolean((args)[1], scriptContext) ? -1 : 0) : 0;
        int intSIMD_Y = args.Info.Count >= 3 ? (JavascriptConversion::ToBoolean((args)[2], scriptContext) ? -1 : 0) : 0;
        int intSIMD_Z = args.Info.Count >= 4 ? (JavascriptConversion::ToBoolean((args)[3], scriptContext) ? -1 : 0) : 0;
        int intSIMD_W = args.Info.Count >= 5 ? (JavascriptConversion::ToBoolean((args)[4], scriptContext) ? -1 : 0) : 0;

        SIMDValue value = SIMDInt32x4Operation::OpBool(intSIMD_X, intSIMD_Y, intSIMD_Z, intSIMD_W);

        return JavascriptSIMDInt32x4::New(&value, scriptContext);
    }

    Var SIMDInt32x4Lib::EntryFromFloat64x2(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2 && JavascriptSIMDFloat64x2::Is(args[1]))
        {
            JavascriptSIMDFloat64x2 *instance = JavascriptSIMDFloat64x2::FromVar(args[1]);
            Assert(instance);

            return JavascriptSIMDInt32x4::FromFloat64x2(instance, scriptContext);
        }
        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"fromFloat64x2");
    }

    Var SIMDInt32x4Lib::EntryFromFloat64x2Bits(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2 && JavascriptSIMDFloat64x2::Is(args[1]))
        {
            JavascriptSIMDFloat64x2 *instance = JavascriptSIMDFloat64x2::FromVar(args[1]);
            Assert(instance);

            return JavascriptSIMDInt32x4::FromFloat64x2Bits(instance, scriptContext);
        }
        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdFloat32x4TypeMismatch, L"fromFloat64x2Bits");
    }

    Var SIMDInt32x4Lib::EntryFromFloat32x4(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2 && JavascriptSIMDFloat32x4::Is(args[1]))
        {
            JavascriptSIMDFloat32x4 *instance = JavascriptSIMDFloat32x4::FromVar(args[1]);
            Assert(instance);

            return JavascriptSIMDInt32x4::FromFloat32x4(instance, scriptContext);
        }
        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"fromFloat32x4");
    }

    Var SIMDInt32x4Lib::EntryFromFloat32x4Bits(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2 && JavascriptSIMDFloat32x4::Is(args[1]))
        {
            JavascriptSIMDFloat32x4 *instance = JavascriptSIMDFloat32x4::FromVar(args[1]);
            Assert(instance);

            return JavascriptSIMDInt32x4::FromFloat32x4Bits(instance, scriptContext);
        }
        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdFloat32x4TypeMismatch, L"fromFloat32x4Bits");
    }

     // withFlagX/Y/Z/W
    Var SIMDInt32x4Lib::EntryWithFlagX(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // withFlagX(t, value)
        // if value arg is missing, then it is undefined, which is false by javascript semantics
        // t arg has to be int32x4, so cannot be missing.

        if (args.Info.Count >= 2 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            JavascriptSIMDInt32x4 *instance = JavascriptSIMDInt32x4::FromVar(args[1]);
            Assert(instance);

            Var value = args.Info.Count >= 3 ? args[2] : scriptContext->GetLibrary()->GetUndefined();

            return instance->CopyAndSetLaneFlag(SIMD_X, JavascriptConversion::ToBoolean(value, scriptContext), scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"withFlagX");
    }

    Var SIMDInt32x4Lib::EntryWithFlagY(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // withFlagY(t, value)
        // if value arg is missing, then it is undefined, which is false by javascript semantics
        // t arg has to be int32x4, so cannot be missing.

        if (args.Info.Count >= 2 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            JavascriptSIMDInt32x4 *instance = JavascriptSIMDInt32x4::FromVar(args[1]);
            Assert(instance);

            Var value = args.Info.Count >= 3 ? args[2] : scriptContext->GetLibrary()->GetUndefined();

            return instance->CopyAndSetLaneFlag(SIMD_Y, JavascriptConversion::ToBoolean(value, scriptContext), scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"withFlagY");
    }

    Var SIMDInt32x4Lib::EntryWithFlagZ(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // withFlagZ(t, value)
        // if value arg is missing, then it is undefined, which is false by javascript semantics
        // t arg has to be int32x4, so cannot be missing.

        if (args.Info.Count >= 2 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            JavascriptSIMDInt32x4 *instance = JavascriptSIMDInt32x4::FromVar(args[1]);
            Assert(instance);

            Var value = args.Info.Count >= 3 ? args[2] : scriptContext->GetLibrary()->GetUndefined();

            return instance->CopyAndSetLaneFlag(SIMD_Z, JavascriptConversion::ToBoolean(value, scriptContext), scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"withFlagZ");
    }

    Var SIMDInt32x4Lib::EntryWithFlagW(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // withFlagW(t, value)
        // if value arg is missing, then it is undefined, which is false by javascript semantics
        // t arg has to be int32x4, so cannot be missing.

        if (args.Info.Count >= 2 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            JavascriptSIMDInt32x4 *instance = JavascriptSIMDInt32x4::FromVar(args[1]);
            Assert(instance);

            Var value = args.Info.Count >= 3 ? args[2] : scriptContext->GetLibrary()->GetUndefined();

            return instance->CopyAndSetLaneFlag(SIMD_W, JavascriptConversion::ToBoolean(value, scriptContext), scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"withFlagW");
    }
    //Lane Access
    Var SIMDInt32x4Lib::EntryExtractLane(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // first arg has to be of type Int32x4, so cannot be missing.
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            // if value arg is missing, then it is undefined.
            Var laneVar = args.Info.Count >= 3 ? args[2] : scriptContext->GetLibrary()->GetUndefined();
            int result = SIMD128ExtractLane<JavascriptSIMDInt32x4, 4, int>(args[1], laneVar, scriptContext);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"ExtractLane");
    }

    Var SIMDInt32x4Lib::EntryReplaceLane(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // first arg has to be of type Int32x4, so cannot be missing.
        if (args.Info.Count >= 4 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            // if value arg is missing, then it is undefined.
            Var laneVar = args.Info.Count >= 4 ? args[2] : scriptContext->GetLibrary()->GetUndefined();
            Var argVal = args.Info.Count >= 4 ? args[3] : scriptContext->GetLibrary()->GetUndefined();
            int value = JavascriptConversion::ToInt32(argVal, scriptContext);

            SIMDValue result = SIMD128ReplaceLane<JavascriptSIMDInt32x4, 4, int>(args[1], laneVar, value, scriptContext);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"ReplaceLane");
    }
    Var SIMDInt32x4Lib::EntryAbs(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            Assert(a);

            SIMDValue value, result;

            value = a->GetValue();
            result = SIMDInt32x4Operation::OpAbs(value);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"abs");
    }

    Var SIMDInt32x4Lib::EntryNeg(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            Assert(a);

            SIMDValue value, result;

            value = a->GetValue();
            result = SIMDInt32x4Operation::OpNeg(value);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"neg");
    }

    Var SIMDInt32x4Lib::EntryNot(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            Assert(a);

            SIMDValue value, result;

            value = a->GetValue();
            result = SIMDInt32x4Operation::OpNot(value);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"not");
    }

    Var SIMDInt32x4Lib::EntryAdd(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]) && JavascriptSIMDInt32x4::Is(args[2]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            JavascriptSIMDInt32x4 *b = JavascriptSIMDInt32x4::FromVar(args[2]);
            Assert(a && b);

            SIMDValue result, aValue, bValue;

            aValue = a->GetValue();
            bValue = b->GetValue();
            result = SIMDInt32x4Operation::OpAdd(aValue, bValue);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"add");
    }

    Var SIMDInt32x4Lib::EntrySub(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]) && JavascriptSIMDInt32x4::Is(args[2]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            JavascriptSIMDInt32x4 *b = JavascriptSIMDInt32x4::FromVar(args[2]);
            Assert(a && b);

            SIMDValue result, aValue, bValue;

            aValue = a->GetValue();
            bValue = b->GetValue();
            result = SIMDInt32x4Operation::OpSub(aValue, bValue);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"sub");
    }

    Var SIMDInt32x4Lib::EntryMul(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]) && JavascriptSIMDInt32x4::Is(args[2]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            JavascriptSIMDInt32x4 *b = JavascriptSIMDInt32x4::FromVar(args[2]);
            Assert(a && b);

            SIMDValue result, aValue, bValue;

            aValue = a->GetValue();
            bValue = b->GetValue();
            result = SIMDInt32x4Operation::OpMul(aValue, bValue);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"mul");
    }

    Var SIMDInt32x4Lib::EntryAnd(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]) && JavascriptSIMDInt32x4::Is(args[2]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            JavascriptSIMDInt32x4 *b = JavascriptSIMDInt32x4::FromVar(args[2]);
            Assert(a && b);

            SIMDValue result, aValue, bValue;

            aValue = a->GetValue();
            bValue = b->GetValue();
            result = SIMDInt32x4Operation::OpAnd(aValue, bValue);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"and");
    }

    Var SIMDInt32x4Lib::EntryOr(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]) && JavascriptSIMDInt32x4::Is(args[2]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            JavascriptSIMDInt32x4 *b = JavascriptSIMDInt32x4::FromVar(args[2]);
            Assert(a && b);

            SIMDValue result, aValue, bValue;

            aValue = a->GetValue();
            bValue = b->GetValue();
            result = SIMDInt32x4Operation::OpOr(aValue, bValue);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"or");
    }

    Var SIMDInt32x4Lib::EntryXor(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]) && JavascriptSIMDInt32x4::Is(args[2]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            JavascriptSIMDInt32x4 *b = JavascriptSIMDInt32x4::FromVar(args[2]);
            Assert(a && b);

            SIMDValue result, aValue, bValue;

            aValue = a->GetValue();
            bValue = b->GetValue();
            result = SIMDInt32x4Operation::OpXor(aValue, bValue);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"xor");
    }

    Var SIMDInt32x4Lib::EntryMin(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]) && JavascriptSIMDInt32x4::Is(args[2]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            JavascriptSIMDInt32x4 *b = JavascriptSIMDInt32x4::FromVar(args[2]);
            Assert(a && b);

            SIMDValue result, aValue, bValue;

            aValue = a->GetValue();
            bValue = b->GetValue();
            result = SIMDInt32x4Operation::OpMin(aValue, bValue);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"min");
    }

    Var SIMDInt32x4Lib::EntryMax(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]) && JavascriptSIMDInt32x4::Is(args[2]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            JavascriptSIMDInt32x4 *b = JavascriptSIMDInt32x4::FromVar(args[2]);
            Assert(a && b);

            SIMDValue result, aValue, bValue;

            aValue = a->GetValue();
            bValue = b->GetValue();
            result = SIMDInt32x4Operation::OpMax(aValue, bValue);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"max");
    }

    Var SIMDInt32x4Lib::EntryLessThan(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        // strict type on both operands
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]) && JavascriptSIMDInt32x4::Is(args[2]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            JavascriptSIMDInt32x4 *b = JavascriptSIMDInt32x4::FromVar(args[2]);
            Assert(a && b);

            SIMDValue result, aValue, bValue;

            aValue = a->GetValue();
            bValue = b->GetValue();

            result = SIMDInt32x4Operation::OpLessThan(aValue, bValue);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"lessThan");
    }

    Var SIMDInt32x4Lib::EntryEqual(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        // strict type on both operands
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]) && JavascriptSIMDInt32x4::Is(args[2]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            JavascriptSIMDInt32x4 *b = JavascriptSIMDInt32x4::FromVar(args[2]);
            Assert(a && b);

            SIMDValue result, aValue, bValue;

            aValue = a->GetValue();
            bValue = b->GetValue();

            result = SIMDInt32x4Operation::OpEqual(aValue, bValue);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"equal");
    }

    Var SIMDInt32x4Lib::EntryGreaterThan(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        // strict type on both operands
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]) && JavascriptSIMDInt32x4::Is(args[2]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            JavascriptSIMDInt32x4 *b = JavascriptSIMDInt32x4::FromVar(args[2]);
            Assert(a && b);

            SIMDValue result, aValue, bValue;

            aValue = a->GetValue();
            bValue = b->GetValue();

            result = SIMDInt32x4Operation::OpGreaterThan(aValue, bValue);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"greaterThan");
    }

    Var SIMDInt32x4Lib::EntrySwizzle(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        // strict type on both operands
        if (args.Info.Count >= 2 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            // type check on lane indices
            if (args.Info.Count < 6)
            {
                // missing lane args
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedNumber, L"Lane index");
            }

            Var lane0 = args[2];
            Var lane1 = args[3];
            Var lane2 = args[4];
            Var lane3 = args[5];

            return SIMD128SlowShuffle<JavascriptSIMDInt32x4>(args[1], args[1], lane0, lane1, lane2, lane3, 4, scriptContext);
        }
        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"swizzle");
    }

    Var SIMDInt32x4Lib::EntryShuffle(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        // If any of the args are missing, then it is Undefined type which causes TypeError exception.
        // strict type on both operands
        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]) && JavascriptSIMDInt32x4::Is(args[2]))
        {
            // type check on lane indices
            if (args.Info.Count < 7)
            {
                // missing lane args
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedNumber, L"Lane index");
            }

            Var lane0 = args[3];
            Var lane1 = args[4];
            Var lane2 = args[5];
            Var lane3 = args[6];

            return SIMD128SlowShuffle<JavascriptSIMDInt32x4>(args[1], args[2], lane0, lane1, lane2, lane3, 8, scriptContext);

        }
        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"shuffle");
    }

    Var SIMDInt32x4Lib::EntryShiftLeft(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            Assert(a);

            SIMDValue result, aValue;

            aValue = a->GetValue();
            Var countVar = args[2]; // {int} bits Bit count
            int32 count = JavascriptConversion::ToInt32(countVar, scriptContext);

            result = SIMDInt32x4Operation::OpShiftLeft(aValue, count);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"shiftLeft");
    }

    Var SIMDInt32x4Lib::EntryShiftRightLogical(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            Assert(a);

            SIMDValue result, aValue;

            aValue = a->GetValue();
            Var countVar = args[2]; // {int} bits Bit count
            int32 count = JavascriptConversion::ToInt32(countVar, scriptContext);

            result = SIMDInt32x4Operation::OpShiftRightLogical(aValue, count);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"shiftRightLogical");
    }

    Var SIMDInt32x4Lib::EntryShiftRightArithmetic(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 3 && JavascriptSIMDInt32x4::Is(args[1]))
        {
            JavascriptSIMDInt32x4 *a = JavascriptSIMDInt32x4::FromVar(args[1]);
            Assert(a);

            SIMDValue result, aValue;

            aValue = a->GetValue();
            Var countVar = args[2]; // {int} bits Bit count
            int32 count = JavascriptConversion::ToInt32(countVar, scriptContext);

            result = SIMDInt32x4Operation::OpShiftRightArithmetic(aValue, count);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"shiftRightArithmetic");
    }

    Var SIMDInt32x4Lib::EntrySelect(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 4 && JavascriptSIMDInt32x4::Is(args[1]) &&
            JavascriptSIMDInt32x4::Is(args[2]) && JavascriptSIMDInt32x4::Is(args[3]))
        {
            JavascriptSIMDInt32x4 *m = JavascriptSIMDInt32x4::FromVar(args[1]);
            JavascriptSIMDInt32x4 *t = JavascriptSIMDInt32x4::FromVar(args[2]);
            JavascriptSIMDInt32x4 *f = JavascriptSIMDInt32x4::FromVar(args[3]);
            Assert(m && t && f);

            SIMDValue result, maskValue, trueValue, falseValue;

            maskValue   = m->GetValue();
            trueValue   = t->GetValue();
            falseValue  = f->GetValue();

            result = SIMDInt32x4Operation::OpSelect(maskValue, trueValue, falseValue);

            return JavascriptSIMDInt32x4::New(&result, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"select");
    }

    Var SIMDInt32x4Lib::EntryLoad(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        return SIMD128TypedArrayLoad<JavascriptSIMDInt32x4>(args[1], args[2], 4 * TySize[TyInt32], scriptContext);
    }

    Var SIMDInt32x4Lib::EntryLoad1(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        return SIMD128TypedArrayLoad<JavascriptSIMDInt32x4>(args[1], args[2], 1 * TySize[TyInt32], scriptContext);
    }

    Var SIMDInt32x4Lib::EntryLoad2(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        return SIMD128TypedArrayLoad<JavascriptSIMDInt32x4>(args[1], args[2], 2 * TySize[TyInt32], scriptContext);
    }

    Var SIMDInt32x4Lib::EntryLoad3(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        return SIMD128TypedArrayLoad<JavascriptSIMDInt32x4>(args[1], args[2], 3 * TySize[TyInt32], scriptContext);
    }

    Var SIMDInt32x4Lib::EntryStore(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 4 && JavascriptSIMDInt32x4::Is(args[3]))
        {
            SIMD128TypedArrayStore<JavascriptSIMDInt32x4>(args[1], args[2], args[3], 4 * TySize[TyInt32], scriptContext);
            return NULL;
        }
        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInvalidArgType, L"SIMD.Int32x4.store");
    }

    Var SIMDInt32x4Lib::EntryStore1(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 4 && JavascriptSIMDInt32x4::Is(args[3]))
        {
            SIMD128TypedArrayStore<JavascriptSIMDInt32x4>(args[1], args[2], args[3], 1 * TySize[TyInt32], scriptContext);
            return NULL;
        }
        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInvalidArgType, L"SIMD.Int32x4.store");
    }

    Var SIMDInt32x4Lib::EntryStore2(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 4 && JavascriptSIMDInt32x4::Is(args[3]))
        {
            SIMD128TypedArrayStore<JavascriptSIMDInt32x4>(args[1], args[2], args[3], 2 * TySize[TyInt32], scriptContext);
            return NULL;
        }
        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInvalidArgType, L"SIMD.Int32x4.store");
    }

    Var SIMDInt32x4Lib::EntryStore3(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 4 && JavascriptSIMDInt32x4::Is(args[3]))
        {
            SIMD128TypedArrayStore<JavascriptSIMDInt32x4>(args[1], args[2], args[3], 3 * TySize[TyInt32], scriptContext);
            return NULL;
        }
        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInvalidArgType, L"SIMD.Int32x4.store");
    }
}
#endif

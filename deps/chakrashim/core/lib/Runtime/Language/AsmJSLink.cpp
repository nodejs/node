//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"
#include "Library\BoundFunction.h"

#ifndef TEMP_DISABLE_ASMJS
namespace Js{
    bool ASMLink::CheckArrayBuffer(ScriptContext* scriptContext, Var bufferView, const AsmJsModuleInfo * info)
    {
        if (!bufferView)
        {
            return true;
        }

        if (!JavascriptArrayBuffer::Is(bufferView))
        {
            AsmJSCompiler::OutputError(scriptContext, L"Asm.js Runtime Error : Buffer parameter is not an Array buffer");
            return false;
        }
        JavascriptArrayBuffer* buffer = (JavascriptArrayBuffer*)bufferView;
        if (buffer->GetByteLength() <= info->GetMaxHeapAccess())
        {
            AsmJSCompiler::OutputError(scriptContext, L"Asm.js Runtime Error : Buffer bytelength is smaller than constant accesses");
            return false;
        }
        if (info->GetUsesChangeHeap())
        {
            if (buffer->GetByteLength() < 0x1000000)
            {
                Output::Print(L"Asm.js Runtime Error : Buffer bytelength is not a valid size for asm.js\n");
                return false;
            }
            if (info->GetMaxHeapAccess() >= 0x1000000)
            {
                Output::Print(L"Asm.js Runtime Error : Cannot have such large constant accesses\n");
                return false;
            }
        }

        if (!buffer->IsValidAsmJsBufferLength(buffer->GetByteLength(), true))
        {
            AsmJSCompiler::OutputError(scriptContext, L"Asm.js Runtime Error : Buffer bytelength is not a valid size for asm.js");
            return false;
        }

        return true;
    }

    bool ASMLink::CheckFFI(ScriptContext* scriptContext, AsmJsModuleInfo* info, const Var foreign)
    {
        if (info->GetFunctionImportCount() == 0 && info->GetVarImportCount() == 0)
        {
            return true;
        }
        Assert(foreign);
        if (!RecyclableObject::Is(foreign))
        {
            AsmJSCompiler::OutputError(scriptContext, L"Asm.js Runtime Error : FFI is not an object");
            return false;
        }
        TypeId foreignObjType = RecyclableObject::FromVar(foreign)->GetTypeId();
        if (StaticType::Is(foreignObjType) || TypeIds_Proxy == foreignObjType)
        {
            AsmJSCompiler::OutputError(scriptContext, L"Asm.js Runtime Error : FFI is not an object");
            return false;
        }
        return true;
    }

    bool ASMLink::CheckStdLib(ScriptContext* scriptContext, const AsmJsModuleInfo* info, const Var stdlib)
    {
        BVStatic<ASMMATH_BUILTIN_SIZE> mathBuiltinUsed = info->GetAsmMathBuiltinUsed();
        BVStatic<ASMARRAY_BUILTIN_SIZE> arrayBuiltinUsed = info->GetAsmArrayBuiltinUsed();
        if (mathBuiltinUsed.IsAllClear() && arrayBuiltinUsed.IsAllClear())
        {
            return true;
        }
        Assert(stdlib);
        if (!RecyclableObject::Is(stdlib))
        {
            AsmJSCompiler::OutputError(scriptContext, L"Asm.js Runtime Error : StdLib is not an object");
            return false;
        }
        TypeId stdLibObjType = RecyclableObject::FromVar(stdlib)->GetTypeId();
        if (StaticType::Is(stdLibObjType) || TypeIds_Proxy == stdLibObjType)
        {
            AsmJSCompiler::OutputError(scriptContext, L"Asm.js Runtime Error : StdLib is not an object");
            return false;
        }

        Js::JavascriptLibrary* library = scriptContext->GetLibrary();
        if (mathBuiltinUsed.Test(AsmJSMathBuiltinFunction::AsmJSMathBuiltin_infinity))
        {
            Var asmInfinityObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Infinity, scriptContext);
            if (!JavascriptConversion::SameValue(asmInfinityObj, library->GetPositiveInfinite()))
            {
                AsmJSCompiler::OutputError(scriptContext, L"Asm.js Runtime Error : Math constant Infinity is invalid");
                return false;
            }
        }
        if (mathBuiltinUsed.Test(AsmJSMathBuiltinFunction::AsmJSMathBuiltin_nan))
        {
            Var asmNaNObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::NaN, scriptContext);
            if (!JavascriptConversion::SameValue(asmNaNObj, library->GetNaN()))
            {
                AsmJSCompiler::OutputError(scriptContext, L"Asm.js Runtime Error : Math constant NaN is invalid");
                return false;
            }
        }
        Var asmMathObject = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Math, scriptContext);
        for (int i = 0; i < AsmJSMathBuiltinFunction::AsmJSMathBuiltin_COUNT; i++)
        {
            //check if bit is set
            if (!mathBuiltinUsed.Test(i) || i == AsmJSMathBuiltinFunction::AsmJSMathBuiltin_infinity || i == AsmJSMathBuiltinFunction::AsmJSMathBuiltin_nan)
            {
                continue;
            }
            AsmJSMathBuiltinFunction mathBuiltinFunc = (AsmJSMathBuiltinFunction)i;
            if (!CheckMathLibraryMethod(scriptContext, asmMathObject, mathBuiltinFunc))
            {
                AsmJSCompiler::OutputError(scriptContext, L"Asm.js Runtime Error : Math builtin function is invalid");
                return false;
            }
        }
        for (int i = 0; i < AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_COUNT; i++)
        {
            //check if bit is set
            if (!arrayBuiltinUsed.Test(i))
            {
                continue;
            }
            AsmJSTypedArrayBuiltinFunction arrayBuiltinFunc = (AsmJSTypedArrayBuiltinFunction)i;
            if (!CheckArrayLibraryMethod(scriptContext, stdlib, arrayBuiltinFunc))
            {
                AsmJSCompiler::OutputError(scriptContext, L"Asm.js Runtime Error : Array builtin function is invalid");
                return false;
            }
        }
        return true;
    }

    bool ASMLink::CheckArrayLibraryMethod(ScriptContext* scriptContext, const Var stdlib, const AsmJSTypedArrayBuiltinFunction arrayLibMethod)
    {
        Var arrayFuncObj;
        switch (arrayLibMethod)
        {
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_byteLength:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::byteLength, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                if (arrayLibFunc->IsBoundFunction())
                {
                    BoundFunction* boundFunc = (BoundFunction*)arrayLibFunc;
                    RecyclableObject* thisObj = boundFunc->GetBoundThis();
                    if (JavascriptFunction::Is(thisObj))
                    {
                        JavascriptFunction * thisFunc = (JavascriptFunction*)thisObj;
                        if (thisFunc->GetFunctionInfo()->GetOriginalEntryPoint() != (&ArrayBuffer::EntryInfo::GetterByteLength)->GetOriginalEntryPoint())
                        {
                            return false;
                        }
                    }
                    JavascriptFunction* targetFunc = boundFunc->GetTargetFunction();
                    return targetFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&JavascriptFunction::EntryInfo::Call)->GetOriginalEntryPoint();
                }
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Int8Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Int8Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Int8Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Uint8Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Uint8Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Uint8Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Int16Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Int16Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Int16Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Uint16Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Uint16Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Uint16Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Int32Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Int32Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Int32Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Uint32Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Uint32Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Uint32Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Float32Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Float32Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Float32Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Float64Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Float64Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Float64Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        default:
            Assume(UNREACHED);
        }
        return false;
    }

    bool ASMLink::CheckMathLibraryMethod(ScriptContext* scriptContext, const Var asmMathObject, const AsmJSMathBuiltinFunction mathLibMethod)
    {
        Var mathFuncObj;
        switch (mathLibMethod)
        {
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sin:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::sin, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Sin)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_cos:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::cos, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Cos)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_tan:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::tan, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Tan)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_asin:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::asin, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Asin)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_acos:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::acos, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Acos)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_atan:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::atan, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Atan)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ceil:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::ceil, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Ceil)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_floor:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::floor, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Floor)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_exp:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::exp, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Exp)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::log, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Log)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_pow:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::pow, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Pow)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::sqrt, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Sqrt)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_abs:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::abs, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Abs)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_atan2:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::atan2, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Atan2)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_imul:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::imul, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Imul)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_clz32:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::clz32, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Clz32)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_min:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::min, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Min)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_max:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::max, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Max)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;

        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_fround:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::fround, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Fround)->GetOriginalEntryPoint())
                {
                    return true;
                }
            }
            break;

        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_e:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::E, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::E))
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ln10:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::LN10, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::LN10))
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ln2:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::LN2, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::LN2))
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log2e:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::LOG2E, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::LOG2E))
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log10e:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::LOG10E, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::LOG10E))
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_pi:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::PI, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::PI))
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt1_2:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::SQRT1_2, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::SQRT1_2))
                {
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt2:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::SQRT2, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::SQRT2))
                {
                    return true;
                }
            }
            break;
        default:
            Assume(UNREACHED);
        }
        return false;
    }

    bool ASMLink::CheckParams(ScriptContext* scriptContext, AsmJsModuleInfo* info, const Var stdlib, const Var foreign, const Var bufferView)
    {
        if (CheckStdLib(scriptContext, info, stdlib) && CheckArrayBuffer(scriptContext, bufferView, info) && CheckFFI(scriptContext, info, stdlib))
        {
            return true;
        }
        Output::Flush();
        return false;
    }
}
#endif

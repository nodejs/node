//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
// Portions of this file are copyright 2014 Mozilla Foundation, available under the Apache 2.0 license.
//-------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------
// Copyright 2014 Mozilla Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"

#ifndef TEMP_DISABLE_ASMJS
namespace Js
{

    bool ParserWrapper::ParseVarOrConstStatement( AsmJSParser &parser, ParseNode **var )
    {
        Assert( parser );
        *var = nullptr;
        ParseNode *body = parser->sxFnc.pnodeBody;
        if( body )
        {
            ParseNode* lhs = GetBinaryLeft( body );
            ParseNode* rhs = GetBinaryRight( body );
            if( rhs && rhs->nop == knopList )
            {
                AssertMsg( lhs->nop == knopStr, "this should be use asm" );
                *var = rhs;
                return true;
            }
        }
        return false;
    }

    bool ParserWrapper::IsDefinition( ParseNode *arg )
    {
        //TODO, eliminate duplicates
        return true;
    }



    ParseNode* ParserWrapper::NextInList( ParseNode *node )
    {
        Assert( node->nop == knopList );
        return node->sxBin.pnode2;
    }

    ParseNode* ParserWrapper::NextVar( ParseNode *node )
    {
        return node->sxVar.pnodeNext;
    }

    ParseNode* ParserWrapper::FunctionArgsList( ParseNode *node, ArgSlot &numformals )
    {
        Assert( node->nop == knopFncDecl );
        PnFnc func = node->sxFnc;
        ParseNode* first = func.pnodeArgs;
        // throws OOM on uint16 overflow
        for( ParseNode* pnode = first; pnode; pnode = pnode->sxVar.pnodeNext, UInt16Math::Inc(numformals));
        return first;
    }

    PropertyName ParserWrapper::VariableName( ParseNode *node )
    {
        return node->name();
    }

    PropertyName ParserWrapper::FunctionName( ParseNode *node )
    {
        if( node->nop == knopFncDecl )
        {
            PnFnc function = node->sxFnc;
            if( function.pnodeName && function.pnodeName->nop == knopVarDecl )
            {
                return function.pnodeName->sxVar.pid;
            }
        }
        return nullptr;
    }

    ParseNode * ParserWrapper::GetVarDeclList( ParseNode * pnode )
    {
        ParseNode* varNode = pnode;
        while (varNode->nop == knopList)
        {
            ParseNode * var = GetBinaryLeft(varNode);
            if (var->nop == knopVarDecl)
            {
                return var;
            }
            else if (var->nop == knopList)
            {
                var = GetBinaryLeft(var);
                if (var->nop == knopVarDecl)
                {
                    return var;
                }
            }
            varNode = GetBinaryRight(varNode);
        }
        return nullptr;
    }

    void ParserWrapper::ReachEndVarDeclList( ParseNode** outNode )
    {
        ParseNode* pnode = *outNode;
        // moving down to the last var declaration
        while( pnode->nop == knopList )
        {
            ParseNode* var = GetBinaryLeft( pnode );
            if (var->nop == knopVarDecl)
            {
                pnode = GetBinaryRight( pnode );
                continue;
            }
            else if (var->nop == knopList)
            {
                var = GetBinaryLeft( var );
                if (var->nop == knopVarDecl)
                {
                    pnode = GetBinaryRight( pnode );
                    continue;
                }
            }
            break;
        }
        *outNode = pnode;
    }

    AsmJsCompilationException::AsmJsCompilationException( const wchar_t* _msg, ... )
    {
        va_list arglist;
        va_start( arglist, _msg );
        vswprintf_s( msg_, _msg, arglist );
    }

    Var AsmJsChangeHeapBuffer(RecyclableObject * function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count < 1 || !ArrayBuffer::Is(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }


        ArrayBuffer* newArrayBuffer = ArrayBuffer::FromVar(args[1]);
        if (newArrayBuffer->IsDetached() || newArrayBuffer->GetByteLength() & 0xffffff || newArrayBuffer->GetByteLength() <= 0xffffff || newArrayBuffer->GetByteLength() > 0x80000000)
        {
            return JavascriptBoolean::ToVar(FALSE, scriptContext);
        }
        FrameDisplay* frame = ((ScriptFunction*)function)->GetEnvironment();
        Var* moduleArrayBuffer = (Var*)frame->GetItem(0) + AsmJsModuleMemory::MemoryTableBeginOffset;
        *moduleArrayBuffer = newArrayBuffer;
        return JavascriptBoolean::ToVar(TRUE, scriptContext);
    }

#if _M_X64

    // returns an array containing the size of each argument
    uint *GetArgsSizesArray(ScriptFunction* func)
    {
        AsmJsFunctionInfo* info = func->GetFunctionBody()->GetAsmJsFunctionInfo();
        return info->GetArgsSizesArray();
    }

    int GetStackSizeForAsmJsUnboxing(ScriptFunction* func)
    {
        AsmJsFunctionInfo* info = func->GetFunctionBody()->GetAsmJsFunctionInfo();
        int argSize = MachPtr;
        for (ArgSlot i = 0; i < info->GetArgCount(); i++)
        {
            if (info->GetArgType(i).isSIMD())
            {
                argSize += sizeof(AsmJsSIMDValue);
            }
            else
            {
                argSize += MachPtr;
            }
        }
        argSize = ::Math::Align<int32>(argSize, 16);

        if (argSize < 32)
        {
            argSize = 32; // convention is to always allocate spill space for rcx,rdx,r8,r9
        }
        PROBE_STACK_CALL(func->GetScriptContext(), func, argSize);
        return argSize;
    }

    void * UnboxAsmJsArguments(ScriptFunction* func, Var * origArgs, char * argDst, CallInfo callInfo)
    {
        void * address = func->GetEntryPointInfo()->address;
        Assert(address);
        AsmJsFunctionInfo* info = func->GetFunctionBody()->GetAsmJsFunctionInfo();
        ScriptContext* scriptContext = func->GetScriptContext();

        AsmJsModuleInfo::EnsureHeapAttached(func);

        uint actualArgCount = callInfo.Count - 1; // -1 for ScriptFunction
        argDst = argDst + MachPtr; // add one first so as to skip the ScriptFunction argument
        for (ArgSlot i = 0; i < info->GetArgCount(); i++)
        {

            if (info->GetArgType(i).isInt())
            {
                int32 intVal;
                if (i < actualArgCount)
                {
                    intVal = JavascriptMath::ToInt32(*origArgs, scriptContext);
                }
                else
                {
                    intVal = 0;
                }

                *(int64*)(argDst) = 0;
                *(int32*)argDst = intVal;

                argDst = argDst + MachPtr;
            }
            else if (info->GetArgType(i).isFloat())
            {
                float floatVal;
                if (i < actualArgCount)
                {
                    floatVal = (float)(JavascriptConversion::ToNumber(*origArgs, scriptContext));
                }
                else
                {
                    floatVal = (float)(JavascriptNumber::NaN);
                }
                *(int64*)(argDst) = 0;
                *(float*)argDst = floatVal;
                argDst = argDst + MachPtr;
            }
            else if (info->GetArgType(i).isDouble())
            {
                double doubleVal;
                if (i < actualArgCount)
                {
                    doubleVal = JavascriptConversion::ToNumber(*origArgs, scriptContext);
                }
                else
                {
                    doubleVal = JavascriptNumber::NaN;
                }
                *(int64*)(argDst) = 0;
                *(double*)argDst = doubleVal;
                argDst = argDst + MachPtr;
            }
            else if (info->GetArgType(i).isSIMD())
            {
                AsmJsVarType argType = info->GetArgType(i);
                AsmJsSIMDValue simdVal = { 0, 0, 0, 0 };
                // SIMD values are copied unaligned.
                // SIMD values cannot be implicitly coerced from/to other types. If the SIMD parameter is missing (i.e. Undefined), we throw type error since there is not equivalent SIMD value to coerce to.
                switch (argType.which())
                {
                case AsmJsType::Int32x4:
                    if (!JavascriptSIMDInt32x4::Is(*origArgs))
                    {
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"Int32x4");
                    }
                    simdVal = ((JavascriptSIMDInt32x4*)(*origArgs))->GetValue();
                    break;
                case AsmJsType::Float32x4:
                    if (!JavascriptSIMDFloat32x4::Is(*origArgs))
                    {
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdFloat32x4TypeMismatch, L"Float32x4");
                    }
                    simdVal = ((JavascriptSIMDFloat32x4*)(*origArgs))->GetValue();
                    break;
                case AsmJsType::Float64x2:
                    if (!JavascriptSIMDFloat64x2::Is(*origArgs))
                    {
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdFloat64x2TypeMismatch, L"Float64x2");
                    }
                    simdVal = ((JavascriptSIMDFloat64x2*)(*origArgs))->GetValue();
                    break;
                default:
                    Assert(UNREACHED);
                }
                *(AsmJsSIMDValue*)argDst = simdVal;
                argDst = argDst + sizeof(AsmJsSIMDValue);
            }
            ++origArgs;
        }
        // for convenience, lets take the opportunity to return the asm.js entrypoint address
        return address;
    }


    Var BoxAsmJsReturnValue(ScriptFunction* func, int intRetVal, double doubleRetVal, float floatRetVal, __m128 simdRetVal)
    {
        // ExternalEntryPoint doesn't know the return value, so it will send garbage for everything except actual return type
        Var returnValue = nullptr;
        // make call and convert primitive type back to Var
        AsmJsFunctionInfo* info = func->GetFunctionBody()->GetAsmJsFunctionInfo();
        switch (info->GetReturnType().which())
        {
        case AsmJsRetType::Void:
            returnValue = JavascriptOperators::OP_LdUndef(func->GetScriptContext());
            break;
        case AsmJsRetType::Signed:{
            returnValue = JavascriptNumber::ToVar(intRetVal, func->GetScriptContext());
            break;
        }
        case AsmJsRetType::Double:{
            returnValue = JavascriptNumber::New(doubleRetVal, func->GetScriptContext());
            break;
        }
        case AsmJsRetType::Float:{
            returnValue = JavascriptNumber::New(floatRetVal, func->GetScriptContext());
            break;
        }
        case AsmJsRetType::Float32x4:
        {
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDFloat32x4::New(&X86SIMDValue::ToSIMDValue(simdVal), func->GetScriptContext());
            break;
        }
        case AsmJsRetType::Int32x4:
        {
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDInt32x4::New(&X86SIMDValue::ToSIMDValue(simdVal), func->GetScriptContext());
            break;
        }
        case AsmJsRetType::Float64x2:
        {
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDFloat64x2::New(&X86SIMDValue::ToSIMDValue(simdVal), func->GetScriptContext());
            break;
        }
        default:
            Assume(UNREACHED);
        }

        return returnValue;
    }

#elif _M_IX86
    Var AsmJsExternalEntryPoint(RecyclableObject* entryObject, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);
        ScriptFunction* func = (ScriptFunction*)entryObject;
        FunctionBody* body = func->GetFunctionBody();
        AsmJsFunctionInfo* info = body->GetAsmJsFunctionInfo();
        ScriptContext* scriptContext = func->GetScriptContext();
        const uint argInCount = callInfo.Count - 1;
        int argSize = info->GetArgByteSize();
        char* dst;
        Var returnValue = 0;

        AsmJsModuleInfo::EnsureHeapAttached(func);

        argSize = ::Math::Align<int32>(argSize, 8);
        // Allocate stack space for args

        __asm
        {
            sub esp, argSize
            mov dst, esp
        };

        // Unbox Var to primitive type
        {
            int32 intVal; double doubleVal; float floatVal;
            for (ArgSlot i = 0; i < info->GetArgCount(); i++)
            {
                if (info->GetArgType(i).isInt())
                {
                    if (i < argInCount)
                    {
                        intVal = JavascriptMath::ToInt32(args.Values[i + 1], scriptContext);
                    }
                    else
                    {
                        intVal = 0;
                    }
                    *(int32*)dst = intVal;
                    dst += sizeof(int32);
                }
                else if (info->GetArgType(i).isFloat())
                {
                    if (i < argInCount)
                    {
                        floatVal = (float)(JavascriptConversion::ToNumber(args.Values[i + 1], scriptContext));
                    }
                    else
                    {
                        floatVal = (float)(JavascriptNumber::NaN);
                    }
                    *(float*)dst = floatVal;
                    dst += sizeof(float);
                }
                else if (info->GetArgType(i).isDouble())
                {
                    if (i < argInCount)
                    {
                        doubleVal = JavascriptConversion::ToNumber(args.Values[i + 1], scriptContext);
                    }
                    else
                    {
                        doubleVal = JavascriptNumber::NaN;
                    }
                    *(double*)dst = doubleVal;
                    dst += sizeof(double);
                }
                else if (info->GetArgType(i).isSIMD())
                {
                    AsmJsVarType argType = info->GetArgType(i);
                    AsmJsSIMDValue simdVal;
                    // SIMD values are copied unaligned.
                    // SIMD values cannot be implicitly coerced from/to other types. If the SIMD parameter is missing (i.e. Undefined), we throw type error since there is not equivalent SIMD value to coerce to.
                    switch (argType.which())
                    {
                    case AsmJsType::Int32x4:
                        if (i >= argInCount || !JavascriptSIMDInt32x4::Is(args.Values[i + 1]))
                        {
                            JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, L"Int32x4");
                        }
                        simdVal = ((JavascriptSIMDInt32x4*)(args.Values[i + 1]))->GetValue();
                        break;
                    case AsmJsType::Float32x4:
                        if (i >= argInCount || !JavascriptSIMDFloat32x4::Is(args.Values[i + 1]))
                        {
                            JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdFloat32x4TypeMismatch, L"Float32x4");
                        }
                        simdVal = ((JavascriptSIMDFloat32x4*)(args.Values[i + 1]))->GetValue();
                        break;
                    case AsmJsType::Float64x2:
                        if (i >= argInCount || !JavascriptSIMDFloat64x2::Is(args.Values[i + 1]))
                        {
                            JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdFloat64x2TypeMismatch, L"Float64x2");
                        }
                        simdVal = ((JavascriptSIMDFloat64x2*)(args.Values[i + 1]))->GetValue();
                        break;
                    default:
                        Assert(UNREACHED);
                    }
                    *(AsmJsSIMDValue*)dst = simdVal;
                    dst += sizeof(AsmJsSIMDValue);
                }
                else
                {
                    AssertMsg(UNREACHED, "Invalid function arg type.");
                }
            }
        }

        const void * asmJSEntryPoint = func->GetEntryPointInfo()->address;
        // make call and convert primitive type back to Var
        switch (info->GetReturnType().which())
        {
        case AsmJsRetType::Void:
            __asm
            {
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
            }
            returnValue = JavascriptOperators::OP_LdUndef(func->GetScriptContext());
            break;
        case AsmJsRetType::Signed:{
            int32 ival = 0;
            __asm
            {
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                mov ival, eax
            }
            returnValue = JavascriptNumber::ToVar(ival, func->GetScriptContext());
            break;
        }
        case AsmJsRetType::Double:{
            double dval = 0;
            __asm
            {
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                movsd dval, xmm0
            }
            returnValue = JavascriptNumber::New(dval, func->GetScriptContext());
            break;
        }
        case AsmJsRetType::Float:{
            float fval = 0;
            __asm
            {
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                movss fval, xmm0
            }
            returnValue = JavascriptNumber::New((double)fval, func->GetScriptContext());
            break;
        }
        case AsmJsRetType::Int32x4:
            AsmJsSIMDValue simdVal;
            simdVal.Zero();
            __asm
            {
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                movups simdVal, xmm0
            }
            returnValue = JavascriptSIMDInt32x4::New(&simdVal, func->GetScriptContext());
            break;

        case AsmJsRetType::Float32x4:
            simdVal.Zero();
            __asm
            {
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                movups simdVal, xmm0
            }
                returnValue = JavascriptSIMDFloat32x4::New(&simdVal, func->GetScriptContext());
                break;

        case AsmJsRetType::Float64x2:
            simdVal.Zero();
            __asm
            {
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                movups simdVal, xmm0
            }
            returnValue = JavascriptSIMDFloat64x2::New(&simdVal, func->GetScriptContext());
            break;

        default:
            Assume(UNREACHED);
        }
        return returnValue;
    }
#endif

}
#endif

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
#include "ByteCode\Symbol.h"
#include "ByteCode\FuncInfo.h"
#include "ByteCode\ByteCodeWriter.h"
#include "ByteCode\ByteCodeGenerator.h"

namespace Js
{
    bool
    AsmJSCompiler::CheckIdentifier(AsmJsModuleCompiler &m, ParseNode *usepn, PropertyName name)
    {
        if (name == m.GetParser()->names()->arguments || name == m.GetParser()->names()->eval)
        {
            return m.FailName(usepn, L"'%s' is not an allowed identifier", name);
        }
        return true;
    }

    bool
    AsmJSCompiler::CheckModuleLevelName(AsmJsModuleCompiler &m, ParseNode *usepn, PropertyName name)
    {
        if (!CheckIdentifier(m, usepn, name))
        {
            return false;
        }
        if (name == m.GetModuleFunctionName())
        {
            return m.FailName(usepn, L"duplicate name '%s' not allowed", name);
        }
        //Check for all the duplicates here.
        return true;
    }


    bool
    AsmJSCompiler::CheckFunctionHead(AsmJsModuleCompiler &m, ParseNode *fn, bool isGlobal /*= true*/)
    {
        PnFnc fnc = fn->sxFnc;
        if (!fnc.IsSimpleParameterList())
        {
            return m.Fail(fn, L"default & rest args not allowed");
        }
        if (fnc.IsStaticMember())
        {
            return m.Fail(fn, L"static functions are not allowed");
        }

        if (fnc.IsGenerator())
        {
            return m.Fail(fn, L"generator functions are not allowed");
        }

        if (fnc.IsLambda())
        {
            return m.Fail(fn, L"lambda functions are not allowed");
        }

        if (!isGlobal && fnc.nestedCount != 0)
        {
            return m.Fail(fn, L"closure functions are not allowed");
        }

        if (fnc.HasDefaultArguments())
        {
            return m.Fail(fn, L"default arguments not allowed");
        }

        return true;
    }

    bool AsmJSCompiler::CheckTypeAnnotation( AsmJsModuleCompiler &m, ParseNode *coercionNode, AsmJSCoercion *coercion,
        ParseNode **coercedExpr /*= nullptr */)
    {
        switch( coercionNode->nop )
        {
        case knopRsh:
        case knopLsh:
        case knopXor:
        case knopAnd:
        case knopOr: {
            ParseNode *rhs = ParserWrapper::GetBinaryRight( coercionNode );
            *coercion = AsmJS_ToInt32;
            if( coercedExpr )
            {

                if( rhs->nop == knopInt && rhs->sxInt.lw == 0 )
                {
                    if( rhs->nop == knopAnd )
                    {
                        // X & 0 == 0;
                        *coercedExpr = rhs;
                    }
                    else
                    {
                        // (X|0) == (X^0) == (X<<0) == (X>>0) == X
                        *coercedExpr = ParserWrapper::GetBinaryLeft( coercionNode );
                    }
                }
                else
                {
                    *coercedExpr = coercionNode;
                }
            }
            return true;
        }
        case knopPos: {
            *coercion = AsmJS_ToNumber;
            if( coercedExpr )
            {
                *coercedExpr = ParserWrapper::GetUnaryNode( coercionNode );
            }
            return true;
        }
        case knopCall: {
            ParseNode* target;
            AsmJsFunctionDeclaration* sym;
            AsmJsMathFunction* mathSym;
            AsmJsSIMDFunction* simdSym;

            target = coercionNode->sxCall.pnodeTarget;

            if (!target || target->nop != knopName)
            {
                return m.Fail(coercionNode, L"Call must be of the form id(...)");
            }

            simdSym = m.LookupSimdTypeCheck(target->name());
            // var x = f4(ffi.field)
            if (simdSym)
            {
                if (coercionNode->sxCall.argCount == simdSym->GetArgCount())
                {
                    switch (simdSym->GetSimdBuiltInFunction())
                    {
                    case AsmJsSIMDBuiltin_int32x4_check:
                        *coercion = AsmJS_Int32x4;
                        break;
                    case AsmJsSIMDBuiltin_float32x4_check:
                        *coercion = AsmJS_Float32x4;
                        break;
                    case AsmJsSIMDBuiltin_float64x2_check:
                        *coercion = AsmJS_Float64x2;
                        break;
                    default:
                        Assert(UNREACHED);
                    }
                    if (coercedExpr)
                    {
                        *coercedExpr = coercionNode->sxCall.pnodeArgs;
                    }

                    return true;
                }
                else
                {
                    return m.Fail(coercionNode, L"Invalid SIMD coercion");
                }

            }
            // not a SIMD coercion, fall through

            *coercion = AsmJS_FRound;
            sym = m.LookupFunction(target->name());
            mathSym = (AsmJsMathFunction*)sym;

            if (!(mathSym && mathSym->GetMathBuiltInFunction() == AsmJSMathBuiltin_fround))
            {
                return m.Fail( coercionNode, L"call must be to fround coercion" );
            }
            if( coercedExpr )
            {
                *coercedExpr = coercionNode->sxCall.pnodeArgs;
            }
            return true;
        }
        case knopInt:{
            *coercion = AsmJS_ToInt32;
            if( coercedExpr )
            {
                *coercedExpr = coercionNode;
            }
            return true;
        }
        case knopFlt:{
            if (ParserWrapper::IsMinInt(coercionNode))
            {
                *coercion = AsmJS_ToInt32;
            }
            else if (coercionNode->sxFlt.maybeInt)
            {
                return m.Fail(coercionNode, L"Integer literal in return must be in range [-2^31, 2^31)");
            }
            else
            {
                *coercion = AsmJS_ToNumber;
            }
            if( coercedExpr )
            {
                *coercedExpr = coercionNode ;
            }
            return true;
        }
        case knopName:{

            // in this case we are returning a constant var from the global scope
            AsmJsSymbol * constSymSource = m.LookupIdentifier(coercionNode->name());

            if (!constSymSource)
            {
                return m.Fail(coercionNode, L"Identifier not globally declared");
            }

            AsmJsVar * constSrc = constSymSource->Cast<AsmJsVar>();

            if (constSymSource->GetSymbolType() != AsmJsSymbol::Variable || constSrc->isMutable())
            {
                return m.Fail(coercionNode, L"Unannotated variables must be constant");
            }

            if (constSrc->GetType().isSigned())
            {
                *coercion = AsmJS_ToInt32;
            }
            else if (constSrc->GetType().isDouble())
            {
                *coercion = AsmJS_ToNumber;
            }
            else
            {
                Assert(constSrc->GetType().isFloat());
                *coercion = AsmJS_FRound;
            }
            if (coercedExpr)
            {
                *coercedExpr = coercionNode;
            }
            return true;
        }
        default:;
        }

        return m.Fail( coercionNode, L"must be of the form +x, fround(x) or x|0" );
    }

    bool
    AsmJSCompiler::CheckModuleArgument(AsmJsModuleCompiler &m, ParseNode *arg, PropertyName *name, AsmJsModuleArg::ArgType type)
    {
        if (!ParserWrapper::IsDefinition(arg))
        {
            return m.Fail(arg, L"duplicate argument name not allowed");
        }

        if (!CheckIdentifier(m, arg, arg->name()))
        {
            return false;
        }
        *name = arg->name();

        m.GetByteCodeGenerator()->AssignPropertyId(*name);

        AsmJsModuleArg * moduleArg = Anew(m.GetAllocator(), AsmJsModuleArg, *name, type);

        if (!m.DefineIdentifier(*name, moduleArg))
        {
            return m.Fail(arg, L"duplicate argument name not allowed");
        }

        if (!CheckModuleLevelName(m, arg, *name))
        {
            return false;
        }

        return true;
    }

    bool
    AsmJSCompiler::CheckModuleArguments(AsmJsModuleCompiler &m, ParseNode *fn)
    {
        ArgSlot numFormals = 0;

        ParseNode *arg1 = ParserWrapper::FunctionArgsList( fn, numFormals );
        ParseNode *arg2 = arg1 ? ParserWrapper::NextVar( arg1 ) : nullptr;
        ParseNode *arg3 = arg2 ? ParserWrapper::NextVar( arg2 ) : nullptr;

        if (numFormals > 3)
        {
            return m.Fail(fn, L"asm.js modules takes at most 3 argument");
        }

        PropertyName arg1Name = nullptr;
        if (numFormals >= 1 && !CheckModuleArgument(m, arg1, &arg1Name, AsmJsModuleArg::ArgType::StdLib))
        {
            return false;
        }

        m.InitStdLibArgName(arg1Name);

        PropertyName arg2Name = nullptr;
        if (numFormals >= 2 && !CheckModuleArgument(m, arg2, &arg2Name, AsmJsModuleArg::ArgType::Import))
        {
            return false;
        }
        m.InitForeignArgName(arg2Name);

        PropertyName arg3Name = nullptr;
        if (numFormals >= 3 && !CheckModuleArgument(m, arg3, &arg3Name, AsmJsModuleArg::ArgType::Heap))
        {
            return false;
        }
        m.InitBufferArgName(arg3Name);

        return true;
    }

    bool AsmJSCompiler::CheckGlobalVariableImportExpr( AsmJsModuleCompiler &m, PropertyName varName, AsmJSCoercion coercion, ParseNode *coercedExpr )
    {
        if( !ParserWrapper::IsDotMember(coercedExpr) )
        {
            return m.FailName( coercedExpr, L"invalid import expression for global '%s'", varName );
        }
        ParseNode *base = ParserWrapper::DotBase(coercedExpr);
        PropertyName field = ParserWrapper::DotMember(coercedExpr);

        PropertyName importName = m.GetForeignArgName();
        if (!importName || !field)
        {
            return m.Fail(coercedExpr, L"cannot import without an asm.js foreign parameter");
        }
        m.GetByteCodeGenerator()->AssignPropertyId(field);
        if ((base->name() != importName))
        {
            return m.FailName(coercedExpr, L"base of import expression must be '%s'", importName);
        }
        return m.AddGlobalVarImport(varName, field, coercion);
    }

    bool AsmJSCompiler::CheckGlobalVariableInitImport( AsmJsModuleCompiler &m, PropertyName varName, ParseNode *initNode, bool isMutable /*= true*/)
    {
        AsmJSCoercion coercion;
        ParseNode *coercedExpr;
        if( !CheckTypeAnnotation( m, initNode, &coercion, &coercedExpr ) )
        {
            return false;
        }
        if ((ParserWrapper::IsFroundNumericLiteral(coercedExpr)) && coercion == AsmJSCoercion::AsmJS_FRound)
        {
            return m.AddNumericVar(varName, coercedExpr, true, isMutable);
        }
        return CheckGlobalVariableImportExpr( m, varName, coercion, coercedExpr );
    }

    bool AsmJSCompiler::CheckNewArrayView( AsmJsModuleCompiler &m, PropertyName varName, ParseNode *newExpr )
    {
        Assert( newExpr->nop == knopNew );
        ParseNode *ctorExpr = newExpr->sxCall.pnodeTarget;
        ArrayBufferView::ViewType type;
        if( ParserWrapper::IsDotMember(ctorExpr) )
        {
            ParseNode *base = ParserWrapper::DotBase(ctorExpr);

            PropertyName globalName = m.GetStdLibArgName();
            if (!globalName)
            {
                return m.Fail(base, L"cannot create array view without an asm.js global parameter");
            }

            if (!ParserWrapper::IsNameDeclaration(base) || base->name() != globalName)
            {
                return m.FailName(base, L"expecting '%s.*Array", globalName);
            }
            PropertyName fieldName = ParserWrapper::DotMember(ctorExpr);
            if (!fieldName)
            {
                return m.FailName(ctorExpr, L"Failed to define array view to var %s", varName);
            }
            PropertyId field = fieldName->GetPropertyId();

            switch (field)
            {
            case PropertyIds::Int8Array:
                type = ArrayBufferView::TYPE_INT8;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Int8Array);
                break;
            case PropertyIds::Uint8Array:
                type = ArrayBufferView::TYPE_UINT8;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Uint8Array);
                break;
            case PropertyIds::Int16Array:
                type = ArrayBufferView::TYPE_INT16;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Int16Array);
                break;
            case PropertyIds::Uint16Array:
                type = ArrayBufferView::TYPE_UINT16;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Uint16Array);
                break;
            case PropertyIds::Int32Array:
                type = ArrayBufferView::TYPE_INT32;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Int32Array);
                break;
            case PropertyIds::Uint32Array:
                type = ArrayBufferView::TYPE_UINT32;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Uint32Array);
                break;
            case PropertyIds::Float32Array:
                type = ArrayBufferView::TYPE_FLOAT32;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Float32Array);
                break;
            case PropertyIds::Float64Array:
                type = ArrayBufferView::TYPE_FLOAT64;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Float64Array);
                break;
            default:
                return m.Fail(ctorExpr, L"could not match typed array name");
                break;
            }
        }
        else if (ctorExpr->nop == knopName)
        {
            AsmJsSymbol * buffFunc = m.LookupIdentifier(ctorExpr->name());

            if (!buffFunc || buffFunc->GetSymbolType() != AsmJsSymbol::TypedArrayBuiltinFunction)
            {
                return m.Fail(ctorExpr, L"invalid 'new' import");
            }
            type = buffFunc->Cast<AsmJsTypedArrayFunction>()->GetViewType();
            if (type == ArrayBufferView::TYPE_INVALID)
            {
                return m.Fail(ctorExpr, L"could not match typed array name");
            }
        }
        else
        {
            return m.Fail(newExpr, L"invalid 'new' import");
        }

        ParseNode *bufArg = newExpr->sxCall.pnodeArgs;
        if( !bufArg || !ParserWrapper::IsNameDeclaration( bufArg ) )
        {
            return m.Fail( ctorExpr, L"array view constructor takes exactly one argument" );
        }

        PropertyName bufferName = m.GetBufferArgName();
        if( !bufferName )
        {
            return m.Fail( bufArg, L"cannot create array view without an asm.js heap parameter" );
        }

        if( bufferName != bufArg->name() )
        {
            return m.FailName( bufArg, L"argument to array view constructor must be '%s'", bufferName );
        }


        if( !m.AddArrayView( varName, type ) )
        {
            return m.FailName( ctorExpr, L"Failed to define array view to var %s", varName );
        }
        return true;
    }

    bool
    AsmJSCompiler::CheckGlobalDotImport(AsmJsModuleCompiler &m, PropertyName varName, ParseNode *initNode)
    {
        ParseNode *base = ParserWrapper::DotBase(initNode);
        PropertyName field = ParserWrapper::DotMember(initNode);
        if( !field )
        {
            return m.Fail( initNode, L"Global import must be in the form c.x where c is stdlib or foreign and x is a string literal" );
        }
        m.GetByteCodeGenerator()->AssignPropertyId(field);
        PropertyName lib = nullptr;
        if (ParserWrapper::IsDotMember(base))
        {
            lib = ParserWrapper::DotMember(base);
            base = ParserWrapper::DotBase(base);

            if (m.GetScriptContext()->GetConfig()->IsSimdjsEnabled())
            {
                if (!lib || (lib->GetPropertyId() != PropertyIds::Math && lib->GetPropertyId() != PropertyIds::SIMD))
                {
                    return m.FailName(initNode, L"'%s' should be Math or SIMD, as in global.Math.xxxx", field);
                }
            }
            else
            {
                if (!lib || lib->GetPropertyId() != PropertyIds::Math)
                {
                    return m.FailName(initNode, L"'%s' should be Math, as in global.Math.xxxx", field);
                }
            }
        }

        if( ParserWrapper::IsNameDeclaration(base) && base->name() == m.GetStdLibArgName() )
        {

            if (m.GetScriptContext()->GetConfig()->IsSimdjsEnabled())
            {
                if (lib && lib->GetPropertyId() == PropertyIds::SIMD)
                {
                    // global.SIMD.xxx
                    AsmJsSIMDFunction *simdFunc;

                    if (!m.LookupStdLibSIMDName(field->GetPropertyId(), field, &simdFunc))
                    {
                        return m.FailName(initNode, L"'%s' is not standard SIMD builtin", varName);
                    }

                    if (simdFunc->GetName() != nullptr)
                    {
                        OutputMessage(m.GetScriptContext(), DEIT_ASMJS_FAILED, L"Warning: SIMD Builtin already defined for var %s", simdFunc->GetName()->Psz());
                    }
                    simdFunc->SetName(varName);
                    if (!m.DefineIdentifier(varName, simdFunc))
                    {
                        return m.FailName(initNode, L"Failed to define SIMD builtin function to var %s", varName);
                    }
                    m.AddSimdBuiltinUse(simdFunc->GetSimdBuiltInFunction());
                    return true;
                }
            }

            // global.Math.xxx
            MathBuiltin mathBuiltin;
            if (m.LookupStandardLibraryMathName(field, &mathBuiltin))
            {
                switch (mathBuiltin.kind)
                {
                case MathBuiltin::Function:{
                    auto func = mathBuiltin.u.func;
                    if (func->GetName() != nullptr)
                    {
                        OutputMessage(m.GetScriptContext(), DEIT_ASMJS_FAILED, L"Warning: Math Builtin already defined for var %s", func->GetName()->Psz());
                    }
                    func->SetName(varName);
                    if (!m.DefineIdentifier(varName, func))
                    {
                        return m.FailName(initNode, L"Failed to define math builtin function to var %s", varName);
                    }
                    m.AddMathBuiltinUse(func->GetMathBuiltInFunction());
                }
                break;
                case MathBuiltin::Constant:
                    if (!m.AddNumericConst(varName, mathBuiltin.u.cst))
                    {
                        return m.FailName(initNode, L"Failed to define math constant to var %s", varName);
                    }
                    m.AddMathBuiltinUse(mathBuiltin.mathLibFunctionName);
                    break;
                default:
                    Assume(UNREACHED);
                }
                return true;
            }

            TypedArrayBuiltin arrayBuiltin;
            if (m.LookupStandardLibraryArrayName(field, &arrayBuiltin))
            {
                if (arrayBuiltin.mFunc->GetName() != nullptr)
                {
                    OutputMessage(m.GetScriptContext(), DEIT_ASMJS_FAILED, L"Warning: Typed array builtin already defined for var %s", arrayBuiltin.mFunc->GetName()->Psz());
                }
                arrayBuiltin.mFunc->SetName(varName);
                if (!m.DefineIdentifier(varName, arrayBuiltin.mFunc))
                {
                    return m.FailName(initNode, L"Failed to define typed array builtin function to var %s", varName);
                }
                m.AddArrayBuiltinUse(arrayBuiltin.mFunc->GetArrayBuiltInFunction());
                return true;
            }

            return m.FailName(initNode, L"'%s' is not a standard Math builtin", field);
        }
        else if( ParserWrapper::IsNameDeclaration(base) && base->name() == m.GetForeignArgName() )
        {
            // foreign import
            return m.AddModuleFunctionImport( varName, field );
        }
        else if (ParserWrapper::IsNameDeclaration(base))
        {
            // Check if SIMD function import
            // e.g. var x = f4.add
            AsmJsSIMDFunction *simdFunc, *operation;

            simdFunc = m.LookupSimdConstructor(base->name());
            if (simdFunc == nullptr || !m.LookupStdLibSIMDName(simdFunc->GetSimdBuiltInFunction(), field, &operation))
            {
                return m.FailName(initNode, L"Invalid dot expression import. %s is not a standard SIMD operation", varName);
            }

            if (operation->GetName() != nullptr)
            {
                OutputMessage(m.GetScriptContext(), DEIT_ASMJS_FAILED, L"Warning: SIMD Builtin already defined for var %s", operation->GetName()->Psz());
            }

            // bind operation to var
            operation->SetName(varName);
            if (!m.DefineIdentifier(varName, operation))
            {
                return m.FailName(initNode, L"Failed to define SIMD builtin function to var %s", varName);
            }

            m.AddSimdBuiltinUse(operation->GetSimdBuiltInFunction());
            return true;
        }

        return m.Fail(initNode, L"expecting c.y where c is either the global or foreign parameter");
    }

    bool
    AsmJSCompiler::CheckModuleGlobal(AsmJsModuleCompiler &m, ParseNode *var)
    {
        Assert(var->nop == knopVarDecl || var->nop == knopConstDecl);

        bool isMutable = var->nop != knopConstDecl;
        PropertyName name = var->name();

        m.GetByteCodeGenerator()->AssignPropertyId(name);
        if (m.LookupIdentifier(name))
        {
            return m.FailName(var, L"import variable %s names must be unique", name);
        }

        if (!CheckModuleLevelName(m, var, name))
        {
            return false;
        }

        if (!var->sxVar.pnodeInit)
        {
            return m.Fail(var, L"module import needs initializer");
        }

        ParseNode *initNode = var->sxVar.pnodeInit;


        if( ParserWrapper::IsNumericLiteral( initNode ) )
        {
            if (m.AddNumericVar(name, initNode, false, isMutable))
            {
                return true;
            }
            else
            {
                return m.FailName(var, L"Failed to declare numeric var %s", name);
            }
        }


        if (initNode->nop == knopOr || initNode->nop == knopPos || initNode->nop == knopCall)
        {
            // SIMD_JS
            // e.g. var x = f4(1.0, 2.0, 3.0, 4.0)
            if (initNode->nop == knopCall)
            {
                AsmJsSIMDFunction* simdSym;
                // also checks if simd constructor
                simdSym = m.LookupSimdConstructor(initNode->sxCall.pnodeTarget->name());
                // call to simd constructor
                if (simdSym)
                {
                    // validate args and define a SIMD symbol
                    return m.AddSimdValueVar(name, initNode, simdSym);
                }
                // else it is FFI import: var x = f4check(FFI.field), handled in CheckGlobalVariableInitImport
            }

           return CheckGlobalVariableInitImport(m, name, initNode, isMutable );
        }

        if( initNode->nop == knopNew )
        {
           return CheckNewArrayView(m, var->name(), initNode);
        }

        if (ParserWrapper::IsDotMember(initNode))
        {
            return CheckGlobalDotImport(m, name, initNode);
        }


        return m.Fail( initNode, L"Failed to recognize global variable" );
    }

    bool
    AsmJSCompiler::CheckModuleGlobals(AsmJsModuleCompiler &m)
    {
        ParseNode *varStmts;
        if( !ParserWrapper::ParseVarOrConstStatement( m.GetCurrentParserNode(), &varStmts ) )
        {
            return false;
        }

        if (!varStmts)
        {
            return true;
        }
        while (varStmts->nop == knopList)
        {
            ParseNode * pnode = ParserWrapper::GetBinaryLeft(varStmts);
            while (pnode && pnode->nop != knopEndCode)
            {
                ParseNode * decl;
                if (pnode->nop == knopList)
                {
                    decl = ParserWrapper::GetBinaryLeft(pnode);
                    pnode = ParserWrapper::GetBinaryRight(pnode);
                }
                else
                {
                    decl = pnode;
                    pnode = nullptr;
                }

                if (decl->nop == knopFncDecl)
                {
                    goto varDeclEnd;
                }
                else if (decl->nop != knopConstDecl && decl->nop != knopVarDecl)
                {
                    break;
                }

                if (decl->sxVar.pnodeInit && decl->sxVar.pnodeInit->nop == knopArray)
                {
                    // Assume we reached func tables
                    goto varDeclEnd;
                }

                if (!CheckModuleGlobal(m, decl))
                {
                    return false;
                }
            }

            if (ParserWrapper::GetBinaryRight(varStmts)->nop == knopEndCode)
            {
                // this is an error condition, but CheckFunctionsSequential will figure it out
                goto varDeclEnd;
            }
            varStmts = ParserWrapper::GetBinaryRight(varStmts);
        }
varDeclEnd:
        // we will collect information on the function tables now and come back to the functions themselves afterwards,
        // because function table information is used when processing function bodies
        ParseNode * fnNodes = varStmts;

        while (fnNodes->nop != knopEndCode && ParserWrapper::GetBinaryLeft(fnNodes)->nop == knopFncDecl)
        {
            fnNodes = ParserWrapper::GetBinaryRight(fnNodes);
        }

        if (fnNodes->nop == knopEndCode)
        {
            // if there are no function tables, we can just initialize count to 0
            m.SetFuncPtrTableCount(0);
        }
        else
        {
            m.SetCurrentParseNode(fnNodes);
            if (!CheckFunctionTables(m))
            {
                return false;
            }
        }
        // this will move us back to the beginning of the function declarations
        m.SetCurrentParseNode(varStmts);
        return true;
    }


    bool AsmJSCompiler::CheckFunction( AsmJsModuleCompiler &m, ParseNode* fncNode )
    {
        Assert( fncNode->nop == knopFncDecl );

        if( PHASE_TRACE1( Js::ByteCodePhase ) )
        {
            Output::Print( L"  Checking Asm function: %s\n", fncNode->sxFnc.funcInfo->name);
        }

        if( !CheckFunctionHead( m, fncNode, false ) )
        {
            return false;
        }

        AsmJsFunc* func = m.CreateNewFunctionEntry(fncNode);
        if (!func)
        {
            return m.Fail(fncNode, L"      Error creating function entry");
        }
        return true;
    }

    bool AsmJSCompiler::CheckFunctionsSequential( AsmJsModuleCompiler &m )
    {
        AsmJSParser& list = m.GetCurrentParserNode();
        Assert( list->nop == knopList );


        ParseNode* pnode = ParserWrapper::GetBinaryLeft(list);

        while (pnode->nop == knopFncDecl)
        {
            if( !CheckFunction( m, pnode ) )
            {
                return false;
            }

            if(ParserWrapper::GetBinaryRight(list)->nop == knopEndCode)
            {
                break;
            }
            list = ParserWrapper::GetBinaryRight(list);
            pnode = ParserWrapper::GetBinaryLeft(list);
        }

        m.SetCurrentParseNode( list );

        return true;
    }

    bool AsmJSCompiler::CheckFunctionTables(AsmJsModuleCompiler &m)
    {
        AsmJSParser& list = m.GetCurrentParserNode();
        Assert(list->nop == knopList);

        int32 funcPtrTableCount = 0;
        while (list->nop != knopEndCode)
        {
            ParseNode * varStmt = ParserWrapper::GetBinaryLeft(list);
            if (varStmt->nop != knopConstDecl && varStmt->nop != knopVarDecl)
            {
                break;
            }
            if (!varStmt->sxVar.pnodeInit || varStmt->sxVar.pnodeInit->nop != knopArray)
            {
                break;
            }
            const uint tableSize = varStmt->sxVar.pnodeInit->sxArrLit.count;
            if (!::Math::IsPow2(tableSize))
            {
                return m.FailName(varStmt, L"Function table [%s] size must be a power of 2", varStmt->name());
            }
            if (!m.AddFunctionTable(varStmt->name(), tableSize))
            {
                return m.FailName(varStmt, L"Unable to create new function table %s", varStmt->name());
            }

            AsmJsFunctionTable* ftable = (AsmJsFunctionTable*)m.LookupIdentifier(varStmt->name());
            Assert(ftable);
            ParseNode* pnode = varStmt->sxVar.pnodeInit->sxArrLit.pnode1;
            if (pnode->nop == knopList)
            {
                pnode = ParserWrapper::GetBinaryLeft(pnode);
            }
            if (!ParserWrapper::IsNameDeclaration(pnode))
            {
                return m.FailName(pnode, L"Invalid element in function table %s", varStmt->name());
            }
            ++funcPtrTableCount;
            list = ParserWrapper::GetBinaryRight(list);
        }

        m.SetFuncPtrTableCount(funcPtrTableCount);

        m.SetCurrentParseNode(list);
        return true;
    }

    bool AsmJSCompiler::CheckModuleReturn( AsmJsModuleCompiler& m )
    {
        ParseNode* endStmt = m.GetCurrentParserNode();

        Assert( endStmt->nop == knopList );
        ParseNode* node = ParserWrapper::GetBinaryLeft( endStmt );
        ParseNode* endNode = ParserWrapper::GetBinaryRight( endStmt );

        if( node->nop != knopReturn || endNode->nop != knopEndCode )
        {
            return m.Fail( node, L"Only expression after table functions must be a return" );
        }

        ParseNode* objNode = node->sxReturn.pnodeExpr;
        if( objNode->nop != knopObject )
        {
            if( ParserWrapper::IsNameDeclaration( objNode ) )
            {
                PropertyName name = objNode->name();
                AsmJsSymbol* sym = m.LookupIdentifier( name );
                if( !sym )
                {
                    return m.FailName( node, L"Symbol %s not recognized inside module", name );
                }

                if( sym->GetSymbolType() != AsmJsSymbol::ModuleFunction )
                {
                    return m.FailName( node, L"Symbol %s can only be a function of the module", name );
                }

                AsmJsFunc* func = sym->Cast<AsmJsFunc>();
                if( !m.SetExportFunc( func ) )
                {
                    return m.FailName( node, L"Error adding return Symbol %s", name );
                }
                return true;
            }
            return m.Fail( node, L"Module return must be an object or 1 function" );
        }

        ParseNode* objectElement = ParserWrapper::GetUnaryNode(objNode);
        while( objectElement )
        {
            ParseNode* member = nullptr;
            if( objectElement->nop == knopList )
            {
                member = ParserWrapper::GetBinaryLeft( objectElement );
                objectElement = ParserWrapper::GetBinaryRight( objectElement );
            }
            else if( objectElement->nop == knopMember )
            {
                member = objectElement;
                objectElement = nullptr;
            }
            else
            {
                return m.Fail( node, L"Return object must only contain members" );
            }

            if( member )
            {
                ParseNode* field = ParserWrapper::GetBinaryLeft( member );
                ParseNode* value = ParserWrapper::GetBinaryRight( member );
                if( !ParserWrapper::IsNameDeclaration( field ) || !ParserWrapper::IsNameDeclaration( value ) )
                {
                    return m.Fail( node, L"Return object member must be fields" );
                }

                AsmJsSymbol* sym = m.LookupIdentifier( value->name() );
                if( !sym )
                {
                    return m.FailName( node, L"Symbol %s not recognized inside module", value->name() );
                }

                if( sym->GetSymbolType() != AsmJsSymbol::ModuleFunction )
                {
                    return m.FailName( node, L"Symbol %s can only be a function of the module", value->name() );
                }

                AsmJsFunc* func = sym->Cast<AsmJsFunc>();
                if( !m.AddExport( field->name(), func->GetFunctionIndex() ) )
                {
                    return m.FailName( node, L"Error adding return Symbol %s", value->name() );
                }
            }
        }

        return true;
    }

    bool AsmJSCompiler::CheckFuncPtrTables( AsmJsModuleCompiler &m )
    {
        ParseNode *list = m.GetCurrentParserNode();
        if (!list)
        {
            return true;
        }
        while (list->nop != knopEndCode)
        {
            ParseNode * varStmt = ParserWrapper::GetBinaryLeft(list);
            if (varStmt->nop != knopConstDecl && varStmt->nop != knopVarDecl)
            {
                break;
            }

            ParseNode* nodeInit = varStmt->sxVar.pnodeInit;
            if( !nodeInit || nodeInit->nop != knopArray )
            {
                return m.Fail( varStmt, L"Invalid variable after function declaration" );
            }

            PropertyName tableName = varStmt->name();

            AsmJsSymbol* sym = m.LookupIdentifier(tableName);
            if( !sym )
            {
                // func table not used in functions disregard it
            }
            else
            {
                //Check name
                if( sym->GetSymbolType() != AsmJsSymbol::FuncPtrTable )
                {
                    return m.FailName( varStmt, L"Variable %s is already defined", tableName );
                }

                AsmJsFunctionTable* table = sym->Cast<AsmJsFunctionTable>();
                if( table->IsDefined() )
                {
                    return m.FailName( varStmt, L"Multiple declaration of function table %s", tableName );
                }

                // Check content of the array
                uint count = nodeInit->sxArrLit.count;
                if( table->GetSize() != count )
                {
                    return m.FailName( varStmt, L"Invalid size of function table %s", tableName );
                }

                // Set the content of the array in the table
                ParseNode* node = nodeInit->sxArrLit.pnode1;
                uint i = 0;
                while( node )
                {
                    ParseNode* funcNameNode = nullptr;
                    if( node->nop == knopList )
                    {
                        funcNameNode = ParserWrapper::GetBinaryLeft( node );
                        node = ParserWrapper::GetBinaryRight( node );
                    }
                    else
                    {
                        Assert( i + 1 == count );
                        funcNameNode = node;
                        node = nullptr;
                    }

                    if( ParserWrapper::IsNameDeclaration( funcNameNode ) )
                    {
                        AsmJsSymbol* sym = m.LookupIdentifier( funcNameNode->name() );
                        if( !sym || sym->GetSymbolType() != AsmJsSymbol::ModuleFunction )
                        {
                            return m.FailName( varStmt, L"Element in function table %s is not a function", tableName );
                        }
                        AsmJsFunc* func = sym->Cast<AsmJsFunc>();
                        AsmJsRetType retType;
                        if (!table->SupportsArgCall(func->GetArgCount(), func->GetArgTypeArray(), retType))
                        {
                            return m.FailName(funcNameNode, L"Function signatures in table %s do not match", tableName);
                        }
                        if (!table->CheckAndSetReturnType(func->GetReturnType()))
                        {
                            return m.FailName(funcNameNode, L"Function return types in table %s do not match", tableName);
                        }
                        table->SetModuleFunctionIndex( func->GetFunctionIndex(), i );
                        ++i;
                    }
                    else
                    {
                        return m.FailName(funcNameNode, L"Element in function table %s is not a function name", tableName);
                    }
                }

                table->Define();
            }

            list = ParserWrapper::GetBinaryRight(list);
        }

        if( !m.AreAllFuncTableDefined() )
        {
            return m.Fail(list, L"Some function table were used but not defined");
        }

        m.SetCurrentParseNode(list);
        return true;
    }

    bool AsmJSCompiler::CheckModule( ExclusiveContext *cx, AsmJSParser &parser, ParseNode *stmtList )
    {
        AsmJsModuleCompiler m( cx, parser );
        if( !m.Init() )
        {
            return false;
        }
        if( PropertyName moduleFunctionName = ParserWrapper::FunctionName( m.GetModuleFunctionNode() ) )
        {
            if( !CheckModuleLevelName( m, m.GetModuleFunctionNode(), moduleFunctionName ) )
            {
                return false;
            }
            m.InitModuleName( moduleFunctionName );

            if( PHASE_TRACE1( Js::ByteCodePhase ) )
            {
                Output::Print( L"Asm.Js Module [%s] detected, trying to compile\n", moduleFunctionName->Psz() );
            }
        }

        m.AccumulateCompileTime(AsmJsCompilation::Module);

        if( !CheckFunctionHead( m, m.GetModuleFunctionNode() ) )
        {
            goto AsmJsCompilationError;
        }

        if (!CheckModuleArguments(m, m.GetModuleFunctionNode()))
        {
            goto AsmJsCompilationError;
        }

        if (!CheckModuleGlobals(m))
        {
            goto AsmJsCompilationError;
        }

        m.AccumulateCompileTime(AsmJsCompilation::Module);

        if (!CheckFunctionsSequential(m))
        {
            goto AsmJsCompilationError;
        }

        m.AccumulateCompileTime();
        m.InitMemoryOffsets();

        if( !m.CompileAllFunctions() )
        {
            return false;
        }

        m.AccumulateCompileTime(AsmJsCompilation::ByteCode);

        if (!CheckFuncPtrTables(m))
        {
            m.RevertAllFunctions();
            return false;
        }

        m.AccumulateCompileTime();

        if (!CheckModuleReturn(m))
        {
            m.RevertAllFunctions();
            return false;
        }

        m.CommitFunctions();
        m.CommitModule();
        m.AccumulateCompileTime(AsmJsCompilation::Module);

        m.PrintCompileTrace();

        return true;

AsmJsCompilationError:
        ParseNode * moduleNode = m.GetModuleFunctionNode();
        if( moduleNode )
        {
            FunctionBody* body = moduleNode->sxFnc.funcInfo->GetParsedFunctionBody();
            body->ResetByteCodeGenState();
        }

        cx->byteCodeGenerator->Writer()->Reset();
        return false;
    }

    bool AsmJSCompiler::Compile(ExclusiveContext *cx, AsmJSParser parser, ParseNode *stmtList)
    {
        if (!CheckModule(cx, parser, stmtList))
        {
            OutputError(cx->scriptContext, L"Asm.js compilation failed.");
            return false;
        }
        return true;
    }

    void AsmJSCompiler::OutputError(ScriptContext * scriptContext, const wchar * message, ...)
    {
        va_list argptr;
        va_start(argptr, message);
        VOutputMessage(scriptContext, DEIT_ASMJS_FAILED, message, argptr);
    }

    void AsmJSCompiler::OutputMessage(ScriptContext * scriptContext, const DEBUG_EVENT_INFO_TYPE messageType, const wchar * message, ...)
    {
        va_list argptr;
        va_start(argptr, message);
        VOutputMessage(scriptContext, messageType, message, argptr);
    }

    void AsmJSCompiler::VOutputMessage(ScriptContext * scriptContext, const DEBUG_EVENT_INFO_TYPE messageType, const wchar * message, va_list argptr)
    {
        wchar_t buf[2048];
        size_t size;

        size = _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, message, argptr);
        if (size == -1)
        {
            size = 2048;
        }
        scriptContext->RaiseMessageToDebugger(messageType, buf, scriptContext->GetUrl());
        if (PHASE_TRACE1(AsmjsPhase) || PHASE_TESTTRACE1(AsmjsPhase))
        {
            Output::PrintBuffer(buf, size);
            Output::Print(L"\n");
            Output::Flush();
        }
    }
}

#endif

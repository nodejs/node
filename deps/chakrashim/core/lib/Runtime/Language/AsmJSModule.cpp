//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#ifndef TEMP_DISABLE_ASMJS
#include "ByteCode\Symbol.h"
#include "ByteCode\FuncInfo.h"
#include "ByteCode\ByteCodeAPI.h"
#include "ByteCode\ByteCodeWriter.h"
#include "ByteCode\ByteCodeGenerator.h"
#include "ByteCode\AsmJsByteCodeWriter.h"
#include "Language\AsmJsByteCodeGenerator.h"

namespace Js
{

    bool AsmJsModuleCompiler::CompileAllFunctions()
    {
        const int size = mFunctionArray.Count();

        for (int i = 0; i < size; i++)
        {
            AsmJsFunc* func = mFunctionArray.Item(i);

            if (!CompileFunction(func, i))
            {
                // an error occurred in the function, revert state on all asm.js functions
                for (int j = 0; j <= i; j++)
                {
                    RevertFunction(j);
                }
                return false;
            }
            func->Finish();
        }
        return true;
    }


    void AsmJsModuleCompiler::RevertFunction(int funcIndex)
    {
        AsmJsFunc* func = mFunctionArray.Item(funcIndex);
        FunctionBody * funcBody = func->GetFuncBody();
        funcBody->ResetByteCodeGenState();
        funcBody->AddDeferParseAttribute();
        funcBody->SetFunctionParsed(false);
        funcBody->ResetEntryPoint();
        funcBody->SetEntryPoint(funcBody->GetDefaultEntryPointInfo(), GetScriptContext()->DeferredParsingThunk);
        funcBody->SetIsAsmjsMode(false);
        funcBody->SetIsAsmJsFunction(false);
        func->GetFncNode()->sxFnc.funcInfo->byteCodeFunction = func->GetFuncBody();
    }

    void AsmJsModuleCompiler::RevertAllFunctions()
    {
        for (int i = 0; i < mFunctionArray.Count(); i++)
        {
            RevertFunction(i);
        }
    }


    bool AsmJsModuleCompiler::CommitFunctions()
    {
        const int size = mFunctionArray.Count();
        // if changeHeap is defined, it must be first function, so we should skip it
        for (int i = 0; i < size; i++)
        {
            AsmJsFunc* func = mFunctionArray.Item(i);
            FunctionBody* functionBody = func->GetFuncBody();
            AsmJsFunctionInfo* asmInfo = functionBody->AllocateAsmJsFunctionInfo();
            if (i == 0 && mUsesChangeHeap)
            {
                continue;
            }
            const auto& intRegisterSpace = func->GetRegisterSpace<int>();
            const auto& doubleRegisterSpace = func->GetRegisterSpace<double>();
            const auto& floatRegisterSpace = func->GetRegisterSpace<float>();

            if (!asmInfo->Init(func))
            {
                return false;
            }
            asmInfo->SetIsHeapBufferConst(!mUsesChangeHeap);
            asmInfo->SetUsesHeapBuffer(mUsesHeapBuffer);
            int varCount = 0;
            varCount += (int)((intRegisterSpace.GetTotalVarCount() * INT_SLOTS_SPACE) + 0.5);
            varCount += (int)(floatRegisterSpace.GetTotalVarCount() * FLOAT_SLOTS_SPACE + 0.5);
            varCount += doubleRegisterSpace.GetTotalVarCount() * DOUBLE_SLOTS_SPACE;

            if (IsSimdjsEnabled())
            {
                const auto& simdRegisterSpace = func->GetRegisterSpace<AsmJsSIMDValue>();
                varCount += (int)(simdRegisterSpace.GetTotalVarCount() * SIMD_SLOTS_SPACE);
            }

            functionBody->SetOutParamDepth(func->GetMaxArgOutDepth());
            functionBody->SetVarCount(varCount);
            // should be set in EmitOneFunction
            Assert(functionBody->GetIsAsmjsMode());
            Assert(functionBody->GetIsAsmJsFunction());
            ((EntryPointInfo*)functionBody->GetDefaultEntryPointInfo())->SetIsAsmJSFunction(true);
#if _M_IX86
            if (PHASE_ON1(AsmJsJITTemplatePhase) && !Configuration::Global.flags.NoNative)
            {
                AsmJsCodeGenerator* generator = GetScriptContext()->GetAsmJsCodeGenerator();
                AccumulateCompileTime();
                if (!generator)
                {
                    generator = GetScriptContext()->InitAsmJsCodeGenerator();
                }
                Assert( generator );
                generator->CodeGen(functionBody);
                AccumulateCompileTime(AsmJsCompilation::TemplateJIT);
            }
#endif
        }

        return true;
    }

    bool AsmJsModuleCompiler::CommitModule()
    {
        FuncInfo* funcInfo = GetModuleFunctionNode()->sxFnc.funcInfo;
        FunctionBody* functionBody = funcInfo->GetParsedFunctionBody();
        AsmJsModuleInfo* asmInfo = functionBody->AllocateAsmJsModuleInfo();

        int argCount = 0;
        if (mBufferArgName)
        {
            argCount = 3;
        }
        else if (mForeignArgName)
        {
            argCount = 2;
        }
        else if (mStdLibArgName)
        {
            argCount = 1;
        }

        const int functionCount = mFunctionArray.Count();
        const int functionTableCount = mFunctionTableArray.Count();
        const int importFunctionCount = mImportFunctions.GetTotalVarCount();
        asmInfo->SetFunctionCount(functionCount);
        asmInfo->SetFunctionTableCount(functionTableCount);
        asmInfo->SetFunctionImportCount(importFunctionCount);
        asmInfo->SetVarCount(mVarCount);
        asmInfo->SetVarImportCount(mVarImportCount);
        asmInfo->SetArgInCount(argCount);
        asmInfo->SetModuleMemory(mModuleMemory);
        asmInfo->SetAsmMathBuiltinUsed(mAsmMathBuiltinUsedBV);
        asmInfo->SetAsmArrayBuiltinUsed(mAsmArrayBuiltinUsedBV);
        asmInfo->SetUsesChangeHeap(mUsesChangeHeap);
        asmInfo->SetMaxHeapAccess(mMaxHeapAccess);

        if (IsSimdjsEnabled())
        {
            asmInfo->SetAsmSimdBuiltinUsed(mAsmSimdBuiltinUsedBV);
            asmInfo->SetSimdRegCount(mSimdVarSpace.GetTotalVarCount());
        }

        int varCount = 3; // 3 possible arguments

        functionBody->SetInParamsCount(4); // Always set 4 inParams so the memory space is the same (globalEnv,stdlib,foreign,buffer)
        functionBody->SetReportedInParamsCount(4);
        functionBody->SetConstantCount(2); // Return register + Root
        functionBody->CreateConstantTable();
        functionBody->SetVarCount(varCount);
        functionBody->SetIsAsmjsMode(true);
        functionBody->NewObjectLiteral(); // allocate one object literal for the export object

        AsmJSByteCodeGenerator::EmitEmptyByteCode(funcInfo, GetByteCodeGenerator(), GetModuleFunctionNode());

        // Create export module proxy
        asmInfo->SetExportFunctionIndex(mExportFuncIndex);
        asmInfo->SetExportsCount(mExports.Count());
        for (int i = 0; i < mExports.Count(); i++)
        {
            AsmJsModuleExport& exMod = mExports.Item(i);
            auto ex = asmInfo->GetExport(i);
            *ex.id = exMod.id;
            *ex.location = exMod.location;
        }

        int iVar = 0, iVarImp = 0, iFunc = 0, iFuncImp = 0;
        const int size = mModuleEnvironment.Count();
        asmInfo->InitializeSlotMap(size);
        auto slotMap = asmInfo->GetAsmJsSlotMap();
        for (int i = 0; i < size; i++)
        {
            AsmJsSymbol* sym = mModuleEnvironment.GetValueAt(i);
            if (sym)
            {
                AsmJsSlot * slot = RecyclerNewLeaf(GetScriptContext()->GetRecycler(), AsmJsSlot);
                slot->symType = sym->GetSymbolType();
                slotMap->AddNew(sym->GetName()->GetPropertyId(), slot);
                switch (sym->GetSymbolType())
                {
                case AsmJsSymbol::Variable:{
                    AsmJsVar* var = sym->Cast<AsmJsVar>();
                    auto& modVar = asmInfo->GetVar(iVar++);
                    modVar.location = var->GetLocation();
                    modVar.type = var->GetVarType().which();
                    if (var->GetVarType().isInt())
                    {
                        modVar.initialiser.intInit = var->GetIntInitialiser();
                    }
                    else if (var->GetVarType().isFloat())
                    {
                        modVar.initialiser.floatInit = var->GetFloatInitialiser();
                    }
                    else if (var->GetVarType().isDouble())
                    {
                        modVar.initialiser.doubleInit = var->GetDoubleInitialiser();
                    }
                    else if (IsSimdjsEnabled() && var->GetVarType().isSIMD())
                    {
                        modVar.initialiser.simdInit = var->GetSimdConstInitialiser();
                    }
                    else
                    {
                        Assert(UNREACHED);
                    }

                    modVar.isMutable = var->isMutable();

                    slot->location = modVar.location;
                    slot->varType = var->GetVarType().which();
                    slot->isConstVar = !modVar.isMutable;
                    break;
                }
                case AsmJsSymbol::ConstantImport:{
                    AsmJsConstantImport* var = sym->Cast<AsmJsConstantImport>();
                    auto& modVar = asmInfo->GetVarImport(iVarImp++);
                    modVar.location = var->GetLocation();
                    modVar.field = var->GetField()->GetPropertyId();
                    modVar.type = var->GetVarType().which();

                    slot->location = modVar.location;
                    slot->varType = modVar.type;
                    break;
                }
                case AsmJsSymbol::ImportFunction:{
                    AsmJsImportFunction* func = sym->Cast<AsmJsImportFunction>();
                    auto& modVar = asmInfo->GetFunctionImport(iFuncImp++);
                    modVar.location = func->GetFunctionIndex();
                    modVar.field = func->GetField()->GetPropertyId();

                    slot->location = modVar.location;
                    break;
                }
                case AsmJsSymbol::FuncPtrTable:{
                    AsmJsFunctionTable* funcTable = sym->Cast<AsmJsFunctionTable>();
                    const uint size = funcTable->GetSize();
                    const RegSlot index = funcTable->GetFunctionIndex();
                    asmInfo->SetFunctionTableSize(index, size);
                    auto& modTable = asmInfo->GetFunctionTable(index);
                    for (uint j = 0; j < size; j++)
                    {
                        modTable.moduleFunctionIndex[j] = funcTable->GetModuleFunctionIndex(j);
                    }
                    slot->funcTableSize = size;
                    slot->location = index;

                    break;
                }
                case AsmJsSymbol::ModuleFunction:{
                    AsmJsFunc* func = sym->Cast<AsmJsFunc>();
                    auto& modVar = asmInfo->GetFunction(iFunc++);
                    modVar.location = func->GetFunctionIndex();
                    slot->location = modVar.location;
                    break;
                }
                case AsmJsSymbol::ArrayView:
                {
                    AsmJsArrayView * var = sym->Cast<AsmJsArrayView>();
                    slot->viewType = var->GetViewType();
                    break;
                }
                case AsmJsSymbol::ModuleArgument:
                {
                    AsmJsModuleArg * arg = sym->Cast<AsmJsModuleArg>();
                    slot->argType = arg->GetArgType();
                    break;
                }
                // used only for module validation
                case AsmJsSymbol::MathConstant:
                {
                    AsmJsMathConst * constVar = sym->Cast<AsmJsMathConst>();
                    slot->mathConstVal = *constVar->GetVal();
                    break;
                }
                case AsmJsSymbol::MathBuiltinFunction:
                {
                    AsmJsMathFunction * mathFunc = sym->Cast<AsmJsMathFunction>();
                    slot->builtinMathFunc = mathFunc->GetMathBuiltInFunction();
                    break;
                }
                case AsmJsSymbol::TypedArrayBuiltinFunction:
                {
                    AsmJsTypedArrayFunction * mathFunc = sym->Cast<AsmJsTypedArrayFunction>();
                    slot->builtinArrayFunc = mathFunc->GetArrayBuiltInFunction();
                    break;
                }
                case AsmJsSymbol::SIMDBuiltinFunction:
                {
                    AsmJsSIMDFunction * mathFunc = sym->Cast<AsmJsSIMDFunction>();
                    slot->builtinSIMDFunc = mathFunc->GetSimdBuiltInFunction();
                    break;
                }
                default:
                    Assume(UNREACHED);
                }
            }
        }
        return true;
    }

    void AsmJsModuleCompiler::ASTPrepass(ParseNodePtr pnode, AsmJsFunc * func)
    {
        ThreadContext::ProbeCurrentStackNoDispose(Js::Constants::MinStackByteCodeVisitor, GetByteCodeGenerator()->GetScriptContext());

        if (pnode == NULL)
        {
            return;
        }

        switch (pnode->nop) {
        // these first cases do the interesting work
        case knopBreak:
        case knopContinue:
            GetByteCodeGenerator()->AddTargetStmt(pnode->sxJump.pnodeTarget);
            break;

        case knopInt:
            func->AddConst<int>(pnode->sxInt.lw);
            break;
        case knopFlt:
        {
            const double d = pnode->sxFlt.dbl;
            if (ParserWrapper::IsMinInt(pnode))
            {
                func->AddConst<int>((int)d);
            }
            else if (ParserWrapper::IsUnsigned(pnode))
            {
                func->AddConst<int>((int)(uint32)d);
            }
            else
            {
                func->AddConst<double>(d);
            }
            break;
        }
        case knopName:
        {
            GetByteCodeGenerator()->AssignPropertyId(pnode->name());
            AsmJsSymbol * declSym = LookupIdentifier(pnode->name());
            if (declSym)
            {
                if (declSym->GetSymbolType() == AsmJsSymbol::MathConstant)
                {
                    AsmJsMathConst * definition = declSym->Cast<AsmJsMathConst>();
                    Assert(definition->GetType().isDouble());
                    func->AddConst<double>(*definition->GetVal());
                }
                else if (declSym->GetSymbolType() == AsmJsSymbol::Variable && !declSym->isMutable())
                {
                    AsmJsVar * definition = declSym->Cast<AsmJsVar>();
                    switch (definition->GetVarType().which())
                    {
                    case AsmJsVarType::Double:
                        func->AddConst<double>(definition->GetDoubleInitialiser());
                        break;
                    case AsmJsVarType::Float:
                        func->AddConst<float>(definition->GetFloatInitialiser());
                        break;
                    case AsmJsVarType::Int:
                        func->AddConst<int>(definition->GetIntInitialiser());
                        break;
                    default:
                        Assume(UNREACHED);
                    }
                }
            }
            break;
        }
        case knopCall:
        {
            ASTPrepass(pnode->sxCall.pnodeTarget, func);
            bool evalArgs = true;
            if (pnode->sxCall.pnodeTarget->nop == knopName)
            {
                AsmJsFunctionDeclaration* funcDecl = this->LookupFunction(pnode->sxCall.pnodeTarget->name());
                if (funcDecl && funcDecl->GetSymbolType() == AsmJsSymbol::MathBuiltinFunction)
                {
                    AsmJsMathFunction* mathFunc = funcDecl->Cast<AsmJsMathFunction>();
                    if (mathFunc->GetMathBuiltInFunction() == AsmJSMathBuiltin_fround)
                    {
                        switch (pnode->sxCall.pnodeArgs->nop)
                        {
                        case knopFlt:
                            func->AddConst<float>((float)pnode->sxCall.pnodeArgs->sxFlt.dbl);
                            evalArgs = false;
                            break;
                        case knopInt:
                            func->AddConst<float>((float)pnode->sxCall.pnodeArgs->sxInt.lw);
                            evalArgs = false;
                            break;
                        case knopNeg:
                            if (pnode->sxCall.pnodeArgs->sxUni.pnode1->nop == knopInt && pnode->sxCall.pnodeArgs->sxUni.pnode1->sxInt.lw == 0)
                            {
                                func->AddConst<float>(-0.0f);
                                evalArgs = false;
                                break;
                            }
                        }
                    }
                }
                else if (IsSimdjsEnabled())
                {
                    /*
                    Float32x4 operations work on Float reg space.
                    If any of the args is a literal (DoubleLit), we need to have a copy of it in the Float reg space.
                    Note that we may end up with redundant copies in the Double reg space, since we ASTPrepass the args (Fix later ?)
                    */
                    if (funcDecl && funcDecl->GetSymbolType() == AsmJsSymbol::SIMDBuiltinFunction)
                    {
                        AsmJsSIMDFunction* simdFunc = funcDecl->Cast<AsmJsSIMDFunction>();
                        if (simdFunc->IsFloat32x4Func())
                        {
                            ParseNode *argNode, *arg;
                            argNode = arg = pnode->sxCall.pnodeArgs;
                            do
                            {
                                if (argNode->nop == knopList)
                                {
                                    arg = ParserWrapper::GetBinaryLeft(argNode);
                                    argNode = ParserWrapper::GetBinaryRight(argNode);
                                }
                                if (arg->nop == knopFlt)
                                {
                                    func->AddConst<float>((float)arg->sxFlt.dbl);
                                }
                                if (argNode != arg && argNode->nop == knopFlt)
                                { // last arg
                                    func->AddConst<float>((float)argNode->sxFlt.dbl);
                                }
                            } while (argNode->nop == knopList);
                        }
                    }
                }

            }
            if (evalArgs)
            {
                ASTPrepass(pnode->sxCall.pnodeArgs, func);
            }
            break;
        }
        case knopVarDecl:
            GetByteCodeGenerator()->AssignPropertyId(pnode->name());
            ASTPrepass(pnode->sxVar.pnodeInit, func);
            break;
        // all the rest of the cases simply walk the AST
        case knopQmark:
            ASTPrepass(pnode->sxTri.pnode1, func);
            ASTPrepass(pnode->sxTri.pnode2, func);
            ASTPrepass(pnode->sxTri.pnode3, func);
            break;
        case knopList:
            do
            {
                ParseNode * pnode1 = pnode->sxBin.pnode1;
                ASTPrepass(pnode1, func);
                pnode = pnode->sxBin.pnode2;
            } while (pnode->nop == knopList);
            ASTPrepass(pnode, func);
            break;
        case knopFor:
            ASTPrepass(pnode->sxFor.pnodeInit, func);
            ASTPrepass(pnode->sxFor.pnodeCond, func);
            ASTPrepass(pnode->sxFor.pnodeIncr, func);
            ASTPrepass(pnode->sxFor.pnodeBody, func);
            break;
        case knopIf:
            ASTPrepass(pnode->sxIf.pnodeCond, func);
            ASTPrepass(pnode->sxIf.pnodeTrue, func);
            ASTPrepass(pnode->sxIf.pnodeFalse, func);
            break;
        case knopDoWhile:
        case knopWhile:
            ASTPrepass(pnode->sxWhile.pnodeCond, func);
            ASTPrepass(pnode->sxWhile.pnodeBody, func);
            break;
        case knopReturn:
            ASTPrepass(pnode->sxReturn.pnodeExpr, func);
            break;
        case knopBlock:
            ASTPrepass(pnode->sxBlock.pnodeStmt, func);
            break;
        case knopSwitch:
            ASTPrepass(pnode->sxSwitch.pnodeVal, func);
            for (ParseNode *pnodeT = pnode->sxSwitch.pnodeCases; NULL != pnodeT; pnodeT = pnodeT->sxCase.pnodeNext)
            {
                ASTPrepass(pnodeT, func);
            }
            ASTPrepass(pnode->sxSwitch.pnodeBlock, func);
            break;
        case knopCase:
            ASTPrepass(pnode->sxCase.pnodeExpr, func);
            ASTPrepass(pnode->sxCase.pnodeBody, func);
            break;
        case knopComma:
        {
            ParseNode *pnode1 = pnode->sxBin.pnode1;
            if (pnode1->nop == knopComma)
            {
                // avoid recursion on very large comma expressions.
                ArenaAllocator *alloc = GetByteCodeGenerator()->GetAllocator();
                SList<ParseNode*> *rhsStack = Anew(alloc, SList<ParseNode*>, alloc);
                do {
                    rhsStack->Push(pnode1->sxBin.pnode2);
                    pnode1 = pnode1->sxBin.pnode1;
                } while (pnode1->nop == knopComma);
                ASTPrepass(pnode1, func);
                while (!rhsStack->Empty())
                {
                    ParseNode *pnodeRhs = rhsStack->Pop();
                    ASTPrepass(pnodeRhs, func);
                }
                Adelete(alloc, rhsStack);
            }
            else
            {
                ASTPrepass(pnode1, func);
            }
            ASTPrepass(pnode->sxBin.pnode2, func);
            break;
        }
        default:
        {
            uint flags = ParseNode::Grfnop(pnode->nop);
            if (flags&fnopUni)
            {
                ASTPrepass(pnode->sxUni.pnode1, func);
            }
            else if (flags&fnopBin)
            {
                ASTPrepass(pnode->sxBin.pnode1, func);
                ASTPrepass(pnode->sxBin.pnode2, func);
            }
            break;
        }
        }
    }

    void AsmJsModuleCompiler::BindArguments(ParseNode* argList)
    {
        for (ParseNode* pnode = argList; pnode; pnode = pnode->sxVar.pnodeNext)
        {
            GetByteCodeGenerator()->AssignPropertyId(pnode->name());
        }
    }

    bool AsmJsModuleCompiler::CompileFunction(AsmJsFunc * func, int funcIndex)
    {
        ParseNodePtr fncNode = func->GetFncNode();
        ParseNodePtr pnodeBody = nullptr;

        Assert(fncNode->nop == knopFncDecl && fncNode->sxFnc.funcInfo && fncNode->sxFnc.funcInfo->IsDeferred() && fncNode->sxFnc.pnodeBody == NULL);

        Js::ParseableFunctionInfo* deferParseFunction = fncNode->sxFnc.funcInfo->byteCodeFunction;
        Utf8SourceInfo * utf8SourceInfo = deferParseFunction->GetUtf8SourceInfo();
        ULONG grfscr = utf8SourceInfo->GetParseFlags();
        grfscr = grfscr & (~fscrGlobalCode);
        func->SetOrigParseFlags(grfscr);
        deferParseFunction->SetGrfscr(grfscr | (grfscr & ~fscrDeferredFncExpression));
        deferParseFunction->SetSourceInfo(GetByteCodeGenerator()->GetCurrentSourceIndex(),
            fncNode,
            !!(grfscr & fscrEvalCode),
            ((grfscr & fscrDynamicCode) && !(grfscr & fscrEvalCode)));

        deferParseFunction->SetInParamsCount(fncNode->sxFnc.funcInfo->inArgsCount);
        deferParseFunction->SetReportedInParamsCount(fncNode->sxFnc.funcInfo->inArgsCount);

        if (fncNode->sxFnc.pnodeBody == NULL)
        {
            if (GetScriptContext()->GetConfig()->BindDeferredPidRefs() &&
                !PHASE_OFF1(Js::SkipNestedDeferredPhase))
            {
                deferParseFunction->BuildDeferredStubs(fncNode);
            }
        }
        deferParseFunction->SetIsAsmjsMode(true);
        PageAllocator tempPageAlloc(NULL, Js::Configuration::Global.flags);
        Parser ps(GetScriptContext(), FALSE, &tempPageAlloc);
        FunctionBody * funcBody;
        ParseNodePtr parseTree;

        Assert(!deferParseFunction->GetIsStrictMode());
        CompileScriptException se;
        funcBody = deferParseFunction->ParseAsmJs(&ps, &se, &parseTree);

        TRACE_BYTECODE(L"\nDeferred parse %s\n", funcBody->GetDisplayName());
        if (parseTree && parseTree->nop == knopProg)
        {
            auto body = parseTree->sxProg.pnodeBody;
            if (body && body->nop == knopList)
            {
                auto fncDecl = body->sxBin.pnode1;
                if (fncDecl && fncDecl->nop == knopFncDecl)
                {
                    pnodeBody = fncDecl->sxFnc.pnodeBody;
                    func->SetFuncBody(funcBody);
                }
            }
        }
        GetByteCodeGenerator()->PushFuncInfo(L"Start asm.js AST prepass", fncNode->sxFnc.funcInfo);
        fncNode->sxFnc.funcInfo->byteCodeFunction->SetBoundPropertyRecords(GetByteCodeGenerator()->EnsurePropertyRecordList());
        BindArguments(fncNode->sxFnc.pnodeArgs);
        ASTPrepass(pnodeBody, func);
        GetByteCodeGenerator()->PopFuncInfo(L"End asm.js AST prepass");

        fncNode->sxFnc.pnodeBody = pnodeBody;

        if (!pnodeBody)
        {
            // body should never be null if parsing succeeded
            Assert(UNREACHED);
            return Fail(fncNode, L"Function should always have parse nodes");
        }

        // Check if this function requires a bigger Ast
        UpdateMaxAstSize(fncNode->sxFnc.astSize);

        if (funcIndex == 0 && CheckChangeHeap(func))
        {
            fncNode->sxFnc.pnodeBody = NULL;
            return true;
        }

        if (!SetupFunctionArguments(func, pnodeBody))
        {
            // failure message will be printed by SetupFunctionArguments
            fncNode->sxFnc.pnodeBody = NULL;
            return false;
        }

        if (!SetupLocalVariables(func))
        {
            // failure message will be printed by SetupLocalVariables
            fncNode->sxFnc.pnodeBody = NULL;
            return false;
        }

        // now that we have setup the function, we can generate bytecode for it
        AsmJSByteCodeGenerator gen(func, this);
        bool wasEmit = gen.EmitOneFunction();
        fncNode->sxFnc.pnodeBody = NULL;
        return wasEmit;
    }


    bool AsmJsModuleCompiler::SetupFunctionArguments(AsmJsFunc * func, ParseNodePtr pnode)
    {
        // Check arguments
        ArgSlot numArguments = 0;
        ParseNode * fncNode = func->GetFncNode();
        ParseNode* argNode = ParserWrapper::FunctionArgsList(fncNode, numArguments);

        if (!func->EnsureArgCount(numArguments))
        {
            return Fail(argNode, L"Cannot have variable number of arguments");
        }

        ArgSlot index = 0;
        while (argNode)
        {
            if (pnode->nop != knopList)
            {
                return Fail(pnode, L"Missing assignment statement for argument");
            }


            if (!ParserWrapper::IsDefinition(argNode))
            {
                return Fail(argNode, L"duplicate argument name not allowed");
            }

            PropertyName argName = argNode->name();
            if (!AsmJSCompiler::CheckIdentifier(*this, argNode, argName))
            {
                return false;
            }

            // creates the variable
            AsmJsVarBase* var = func->DefineVar(argName, true);
            if (!var)
            {
                return Fail(argNode, L"Failed to define var");
            }

            ParseNode* argDefinition = ParserWrapper::GetBinaryLeft(pnode);
            if (argDefinition->nop != knopAsg)
            {
                return Fail(argDefinition, L"Expecting an assignment");
            }

            ParseNode* lhs = ParserWrapper::GetBinaryLeft(argDefinition);
            ParseNode* rhs = ParserWrapper::GetBinaryRight(argDefinition);

#define NodeDefineThisArgument(n,var) (n->nop == knopName && ParserWrapper::VariableName(n)->GetPropertyId() == var->GetName()->GetPropertyId())

            if (!NodeDefineThisArgument(lhs, var))
            {
                return Fail(lhs, L"Defining wrong argument");
            }

            if (rhs->nop == knopPos)
            {
                // unary + => double
                var->SetVarType(AsmJsVarType::Double);
                var->SetLocation(func->AcquireRegister<double>());
                // validate stmt
                ParseNode* argSym = ParserWrapper::GetUnaryNode(rhs);

                if (!NodeDefineThisArgument(argSym, var))
                {
                    return Fail(lhs, L"Defining wrong argument");
                }
            }
            else if (rhs->nop == knopOr)
            {
                var->SetVarType(AsmJsVarType::Int);
                var->SetLocation(func->AcquireRegister<int>());

                ParseNode* argSym = ParserWrapper::GetBinaryLeft(rhs);
                ParseNode* intSym = ParserWrapper::GetBinaryRight(rhs);
                // validate stmt
                if (!NodeDefineThisArgument(argSym, var))
                {
                    return Fail(lhs, L"Defining wrong argument");
                }
                if (intSym->nop != knopInt || intSym->sxInt.lw != 0)
                {
                    return Fail(lhs, L"Or value must be 0 when defining arguments");
                }
            }
            else if (rhs->nop == knopCall)
            {
                if (rhs->sxCall.pnodeTarget->nop != knopName)
                {
                    return Fail(rhs, L"call should be for fround");
                }
                AsmJsFunctionDeclaration* funcDecl = this->LookupFunction(rhs->sxCall.pnodeTarget->name());

                if (!funcDecl)
                    return Fail(rhs, L"Cannot resolve function for argument definition, or wrong function");

                if (funcDecl->GetSymbolType() == AsmJsSymbol::MathBuiltinFunction)
                {
                    AsmJsMathFunction* mathFunc = funcDecl->Cast<AsmJsMathFunction>();
                    if (!(mathFunc && mathFunc->GetMathBuiltInFunction() == AsmJSMathBuiltin_fround))
                    {
                        return Fail(rhs, L"call should be for fround");
                    }
                    var->SetVarType(AsmJsVarType::Float);
                    var->SetLocation(func->AcquireRegister<float>());
                }
                else if (IsSimdjsEnabled() && funcDecl->GetSymbolType() == AsmJsSymbol::SIMDBuiltinFunction)
                {
                    AsmJsSIMDFunction* simdFunc = funcDecl->Cast<AsmJsSIMDFunction>();
                    // x = f4check(x)
                    if (!simdFunc->IsTypeCheck())
                    {
                       return Fail(rhs, L"Invalid SIMD argument type check. E.g. expected x = f4check(x)");
                    }
                    var->SetVarType(simdFunc->GetTypeCheckVarType());
                    // We don't set SIMD args reg location here. We defer that after all function locals are processed.
                    // This allows us to capture all SIMD constants from locals initializations, add them to the register space before we assign registers to args and locals.
                    func->GetSimdVarsList().Add(var);
                }
                else
                {
                    return Fail(rhs, L"Wrong function used for argument definition");
                }

                if (!NodeDefineThisArgument(rhs->sxCall.pnodeArgs, var))
                {
                    return Fail(lhs, L"Defining wrong argument");
                }
            }
            else
            {
                return Fail(rhs, L"arguments are not casted as valid Asm.js type");
            }

            if (PHASE_TRACE1(ByteCodePhase))
            {
                Output::Print(L"    Argument [%s] Valid", argName->Psz());
            }

            if (!func->EnsureArgType(var, index++))
            {
                return Fail(rhs, L"Unexpected argument type");
            }

            argNode = ParserWrapper::NextVar(argNode);
            pnode = ParserWrapper::GetBinaryRight(pnode);
        }

        func->SetBodyNode(pnode);
        return true;
    }

    bool AsmJsModuleCompiler::SetupLocalVariables(AsmJsFunc * func)
    {
        ParseNodePtr pnode = func->GetBodyNode();
        MathBuiltin mathBuiltin;
        AsmJsMathFunction* mathFunc = nullptr;
        AsmJsSIMDFunction* simdFunc = nullptr;
        AsmJsSIMDValue simdValue;
        simdValue.Zero();
        // define all variables
        while (pnode->nop == knopList)
        {
            ParseNode * varNode = ParserWrapper::GetBinaryLeft(pnode);
            while (varNode && varNode->nop != knopEndCode)
            {
                ParseNode * decl;
                if (varNode->nop == knopList)
                {
                    decl = ParserWrapper::GetBinaryLeft(varNode);
                    varNode = ParserWrapper::GetBinaryRight(varNode);
                }
                else
                {
                    decl = varNode;
                    varNode = nullptr;
                }
                // if we have hit a non-declaration, we are done processing the function header
                if (decl->nop != knopVarDecl)
                {
                    goto varDeclEnd;
                }
                ParseNode* pnodeInit = decl->sxVar.pnodeInit;
                AsmJsSymbol * declSym = nullptr;

                mathFunc = nullptr;
                simdFunc = nullptr;

                if (pnodeInit->nop == knopName)
                {
                    declSym = LookupIdentifier(pnodeInit->name(), func);
                    if (!declSym || declSym->isMutable() || (declSym->GetSymbolType() != AsmJsSymbol::Variable && declSym->GetSymbolType() != AsmJsSymbol::MathConstant))
                    {
                        return Fail(decl, L"Var declaration with non-constant");
                    }
                }
                else if (pnodeInit->nop == knopCall)
                {
                    if (pnodeInit->sxCall.pnodeTarget->nop != knopName)
                    {
                        return Fail(decl, L"Var declaration with something else than a literal value|fround call");
                    }
                    AsmJsFunctionDeclaration* funcDecl = this->LookupFunction(pnodeInit->sxCall.pnodeTarget->name());

                    if (!funcDecl)
                        return Fail(pnodeInit, L"Cannot resolve function name");

                    if (funcDecl->GetSymbolType() == AsmJsSymbol::MathBuiltinFunction)
                    {
                        mathFunc = funcDecl->Cast<AsmJsMathFunction>();
                        if (!(mathFunc && mathFunc->GetMathBuiltInFunction() == AsmJSMathBuiltin_fround))
                        {
                            return Fail(decl, L"Var declaration with something else than a literal value|fround call");
                        }
                        if (!ParserWrapper::IsFroundNumericLiteral(pnodeInit->sxCall.pnodeArgs))
                        {
                            return Fail(decl, L"Var declaration with something else than a literal value|fround call");
                        }
                    }
                    else if (IsSimdjsEnabled() && funcDecl->GetSymbolType() == AsmJsSymbol::SIMDBuiltinFunction)
                    {
                        // var x = f4(1.0, 2.0, 3.0, 4.0);
                        simdFunc = funcDecl->Cast<AsmJsSIMDFunction>();
                        if (!ValidateSimdConstructor(pnodeInit, simdFunc, simdValue))
                        {
                            return Fail(varNode, L"Invalid SIMD local declaration");
                        }
                    }
                }
                else if (pnodeInit->nop != knopInt && pnodeInit->nop != knopFlt)
                {
                    return Fail(decl, L"Var declaration with something else than a literal value|fround call");
                }
                if (!AsmJSCompiler::CheckIdentifier(*this, decl, decl->name()))
                {
                    // CheckIdentifier will print failure message
                    return false;
                }

                AsmJsVar* var = (AsmJsVar*)func->DefineVar(decl->name(), false);
                if (!var)
                {
                    return Fail(decl, L"Failed to define var");
                }
                RegSlot loc = Constants::NoRegister;
                if (pnodeInit->nop == knopInt)
                {
                    var->SetVarType(AsmJsVarType::Int);
                    var->SetLocation(func->AcquireRegister<int>());
                    var->SetConstInitialiser(pnodeInit->sxInt.lw);
                    loc = func->GetConstRegister<int>(pnodeInit->sxInt.lw);
                }
                else if (ParserWrapper::IsMinInt(pnodeInit))
                {
                    var->SetVarType(AsmJsVarType::Int);
                    var->SetLocation(func->AcquireRegister<int>());
                    var->SetConstInitialiser(MININT);
                    loc = func->GetConstRegister<int>(MININT);
                }
                else if (ParserWrapper::IsUnsigned(pnodeInit))
                {
                    var->SetVarType(AsmJsVarType::Int);
                    var->SetLocation(func->AcquireRegister<int>());
                    var->SetConstInitialiser((int)((uint32)pnodeInit->sxFlt.dbl));
                    loc = func->GetConstRegister<int>((uint32)pnodeInit->sxFlt.dbl);
                }
                else if (pnodeInit->nop == knopFlt)
                {
                    if (pnodeInit->sxFlt.maybeInt)
                    {
                        return Fail(decl, L"Var declaration with integer literal outside range [-2^31, 2^32)");
                    }
                    var->SetVarType(AsmJsVarType::Double);
                    var->SetLocation(func->AcquireRegister<double>());
                    loc = func->GetConstRegister<double>(pnodeInit->sxFlt.dbl);
                    var->SetConstInitialiser(pnodeInit->sxFlt.dbl);
                }
                else if (pnodeInit->nop == knopName)
                {
                    if (declSym->GetSymbolType() == AsmJsSymbol::Variable)
                    {
                        AsmJsVar * definition = declSym->Cast<AsmJsVar>();
                        switch (definition->GetVarType().which())
                        {
                        case AsmJsVarType::Double:
                            var->SetVarType(AsmJsVarType::Double);
                            var->SetLocation(func->AcquireRegister<double>());
                            var->SetConstInitialiser(definition->GetDoubleInitialiser());
                            break;

                        case AsmJsVarType::Float:
                            var->SetVarType(AsmJsVarType::Float);
                            var->SetLocation(func->AcquireRegister<float>());
                            var->SetConstInitialiser(definition->GetFloatInitialiser());
                            break;

                        case AsmJsVarType::Int:
                            var->SetVarType(AsmJsVarType::Int);
                            var->SetLocation(func->AcquireRegister<int>());
                            var->SetConstInitialiser(definition->GetIntInitialiser());
                            break;

                        default:
                            Assume(UNREACHED);
                        }
                    }
                    else
                    {
                        Assert(declSym->GetSymbolType() == AsmJsSymbol::MathConstant);
                        Assert(declSym->GetType() == AsmJsType::Double);

                        AsmJsMathConst * definition = declSym->Cast<AsmJsMathConst>();

                        var->SetVarType(AsmJsVarType::Double);
                        var->SetLocation(func->AcquireRegister<double>());
                        var->SetConstInitialiser(*definition->GetVal());
                    }
                }
                else if (pnodeInit->nop == knopCall)
                {
                    if (mathFunc)
                    {
                        var->SetVarType(AsmJsVarType::Float);
                        var->SetLocation(func->AcquireRegister<float>());
                        if (pnodeInit->sxCall.pnodeArgs->nop == knopInt)
                        {
                            int iVal = pnodeInit->sxCall.pnodeArgs->sxInt.lw;
                            var->SetConstInitialiser((float)iVal);
                            loc = func->GetConstRegister<float>((float)iVal);
                        }
                        else if (ParserWrapper::IsNegativeZero(pnodeInit->sxCall.pnodeArgs))
                        {
                            var->SetConstInitialiser(-0.0f);
                            loc = func->GetConstRegister<float>(-0.0f);
                        }
                        else
                        {
                            // note: fround((-)NumericLiteral) is explicitly allowed for any range, so we do not need to check for maybeInt
                            Assert(pnodeInit->sxCall.pnodeArgs->nop == knopFlt);
                            float fVal = (float)pnodeInit->sxCall.pnodeArgs->sxFlt.dbl;
                            var->SetConstInitialiser((float)fVal);
                            loc = func->GetConstRegister<float>(fVal);
                        }
                    }
                    else if (IsSimdjsEnabled() && simdFunc)
                    {
                        // simd constructor call
                        // en-register the simdvalue constant first
                        func->AddConst<AsmJsSIMDValue>(simdValue);
                        loc = func->GetConstRegister<AsmJsSIMDValue>(simdValue);
                        var->SetConstInitialiser(simdValue);
                        var->SetVarType(simdFunc->GetConstructorVarType());
                        // add to list. assign register after all constants.
                        func->GetSimdVarsList().Add(var);
                    }
                    else
                    {
                        Assert(UNREACHED);
                    }
                }

                if (loc == Constants::NoRegister && pnodeInit->nop != knopName)
                {
                    return Fail(decl, L"Cannot find Register constant for var");
                }
            }

            if (ParserWrapper::GetBinaryRight(pnode)->nop == knopEndCode)
            {
                break;
            }
            pnode = ParserWrapper::GetBinaryRight(pnode);
        }

        varDeclEnd:
        // this code has to be on all exit-path from the function
        if (IsSimdjsEnabled())
        {
            // Now, assign registers to all SIMD vars after all constants are en-registered.
            for (int i = 0; i < func->GetSimdVarsList().Count(); i++)
            {
                AsmJsVarBase *var = func->GetSimdVarsList().Item(i);
                var->SetLocation(func->AcquireRegister<AsmJsSIMDValue>());
            }
            func->GetSimdVarsList().Reset(); // list not needed anymore
        }
        return true;
    }

    AsmJsFunc* AsmJsModuleCompiler::CreateNewFunctionEntry( ParseNode* pnodeFnc )
    {
        PropertyName name = ParserWrapper::FunctionName( pnodeFnc );
        GetByteCodeGenerator()->AssignPropertyId(name);
        AsmJsFunc* func = Anew( &mAllocator, AsmJsFunc, name, pnodeFnc, &mAllocator );
        if( func )
        {
            if( DefineIdentifier( name, func ) )
            {
                func->SetFunctionIndex( pnodeFnc->sxFnc.nestedIndex );
                // Add extra check to make sure all the slots between 0 - Count are filled with func;
                mFunctionArray.SetItem( func->GetFunctionIndex(), func );
                return func;
            }
            // Error adding function
            mAllocator.Free( func, sizeof( AsmJsFunc ) );
        }
        // Error allocating a new function
        return nullptr;
    }

    bool AsmJsModuleCompiler::CheckChangeHeap(AsmJsFunc * func)
    {
        ParseNode * fncNode = func->GetFncNode();
        ParseNode * pnodeBody = fncNode->sxFnc.pnodeBody;
        ParseNode * pnodeArgs = fncNode->sxFnc.pnodeArgs;

        // match AST for changeHeap function.
        // it must be defined in the following format (names/whitespace can differ):
        //function changeHeap(newBuffer)
        //{
        //  if (byteLength(newBuffer) & 0xffffff ||
        //      byteLength(newBuffer) <= 0xffffff ||
        //      byteLength(newBuffer) >  0x80000000)
        //      return false;
        //  heap32 = new Int32Array(newBuffer);
        //  ...
        //  buffer = newBuffer;
        //  return true;
        //}

        // ensure function
        if (pnodeBody->nop != knopList || !pnodeArgs || pnodeArgs->nop != knopVarDecl)
        {
            return false;
        }

        // ensure if expression
        ParseNode * ifNode = pnodeBody->sxBin.pnode1;
        if (ifNode->nop != knopIf || ifNode->sxIf.pnodeFalse)
        {
            return false;
        }

        // validate "byteLength(newBuffer) >  0x80000000"
        ParseNode * orNode = ifNode->sxIf.pnodeCond;
        if (orNode->nop != knopLogOr || orNode->sxBin.pnode1->nop != knopLogOr)
        {
            return false;
        }
        ParseNode * cond = orNode->sxBin.pnode2;
        if (cond->nop != knopGt || !CheckByteLengthCall(cond->sxBin.pnode1, pnodeArgs) || cond->sxBin.pnode2->nop != knopFlt || cond->sxBin.pnode2->sxFlt.dbl != 2147483648.0 || !cond->sxBin.pnode2->sxFlt.maybeInt)
        {
            return false;
        }

        // validate "byteLength(newBuffer) <= 0xffffff"
        orNode = orNode->sxBin.pnode1;
        cond = orNode->sxBin.pnode2;
        if (cond->nop != knopLe || !CheckByteLengthCall(cond->sxBin.pnode1, pnodeArgs) || cond->sxBin.pnode2->nop != knopInt || cond->sxBin.pnode2->sxInt.lw != 0x00ffffff)
        {
            return false;
        }

        // validate "byteLength(newBuffer) & 0xffffff"
        cond = orNode->sxBin.pnode1;
        if (cond->nop != knopAnd || !CheckByteLengthCall(cond->sxBin.pnode1, pnodeArgs) || cond->sxBin.pnode2->nop != knopInt || cond->sxBin.pnode2->sxInt.lw != 0x00ffffff)
        {
            return false;
        }
        // validate "return false;"
        cond = ifNode->sxIf.pnodeTrue;
        if (!cond || cond->nop != knopReturn || cond->sxReturn.pnodeExpr->nop != knopFalse)
        {
            return false;
        }

        // validate heap32 = new Int32Array(newBuffer); etc.
        while (!mArrayViews.Empty())
        {
            // all views that were instantiated must be replaced in the order which they were instantiated
            AsmJsArrayView * requiredArrayView = mArrayViews.Dequeue();
            pnodeBody = pnodeBody->sxBin.pnode2;
            if (pnodeBody->nop != knopList)
            {
                return false;
            }
            ParseNode * assignNode = pnodeBody->sxBin.pnode1;
            if (assignNode->nop != knopAsg || assignNode->sxBin.pnode1->nop != knopName)
            {
                return false;
            }
            // validate left hand side
            AsmJsSymbol * actualArraySym = LookupIdentifier(assignNode->sxBin.pnode1->name());
            if (requiredArrayView != actualArraySym)
            {
                return false;
            }

            ParseNode * callNode = assignNode->sxBin.pnode2;
            // validate correct argument is passed
            if (callNode->nop != knopNew || !callNode->sxCall.pnodeArgs || callNode->sxCall.pnodeArgs->nop != knopName || callNode->sxCall.pnodeArgs->name()->GetPropertyId() != pnodeArgs->name()->GetPropertyId() || callNode->sxCall.pnodeTarget->nop != knopName)
            {
                return false;
            }
            // validate correct function is being called
            AsmJsSymbol * callTargetSym = LookupIdentifier(callNode->sxCall.pnodeTarget->name());
            if (!callTargetSym || callTargetSym->GetSymbolType() != AsmJsSymbol::TypedArrayBuiltinFunction)
            {
                return false;
            }
            if (requiredArrayView->GetViewType() != callTargetSym->Cast<AsmJsTypedArrayFunction>()->GetViewType())
            {
                return false;
            }
        }
        pnodeBody = pnodeBody->sxBin.pnode2;
        if (pnodeBody->nop != knopList)
        {
            return false;
        }

        // validate buffer = newBuffer;
        ParseNode * assign = pnodeBody->sxBin.pnode1;
        if (assign->nop != knopAsg || assign->sxBin.pnode1->nop != knopName || !mBufferArgName || mBufferArgName->GetPropertyId() != assign->sxBin.pnode1->name()->GetPropertyId() ||
            assign->sxBin.pnode2->nop != knopName || pnodeArgs->name()->GetPropertyId() != assign->sxBin.pnode2->name()->GetPropertyId())
        {
            return false;
        }
        // validate return true;
        pnodeBody = pnodeBody->sxBin.pnode2;
        if (pnodeBody->nop != knopList || pnodeBody->sxBin.pnode2->nop != knopEndCode ||
            pnodeBody->sxBin.pnode1->nop != knopReturn || !pnodeBody->sxBin.pnode1->sxReturn.pnodeExpr || pnodeBody->sxBin.pnode1->sxReturn.pnodeExpr->nop != knopTrue)
        {
            return false;
        }
        // now we should flag this module as containing changeHeap method
        mUsesChangeHeap = true;
        AsmJSByteCodeGenerator::EmitEmptyByteCode(func->GetFuncInfo(), GetByteCodeGenerator(), fncNode);
        return true;
    }

    bool AsmJsModuleCompiler::CheckByteLengthCall(ParseNode * callNode, ParseNode * bufferDecl)
    {
        if (callNode->nop != knopCall || callNode->sxCall.pnodeTarget->nop != knopName)
        {
            return false;
        }
        AsmJsSymbol* funcDecl = LookupIdentifier(callNode->sxCall.pnodeTarget->name());
        if (!funcDecl || funcDecl->GetSymbolType() != AsmJsSymbol::TypedArrayBuiltinFunction)
        {
            return false;
        }

        AsmJsTypedArrayFunction* arrayFunc = funcDecl->Cast<AsmJsTypedArrayFunction>();
        return callNode->sxCall.argCount == 1 &&
            !callNode->sxCall.isApplyCall &&
            !callNode->sxCall.isEvalCall &&
            callNode->sxCall.spreadArgCount == 0 &&
            arrayFunc->GetArrayBuiltInFunction() == AsmJSTypedArrayBuiltin_byteLength &&
            callNode->sxCall.pnodeArgs->nop == knopName &&
            callNode->sxCall.pnodeArgs->name()->GetPropertyId() == bufferDecl->name()->GetPropertyId();
    }

    bool AsmJsModuleCompiler::Fail( ParseNode* usepn, const wchar *error )
    {
        AsmJSCompiler::OutputError(GetScriptContext(), error);
        return false;
    }

    bool AsmJsModuleCompiler::FailName( ParseNode *usepn, const wchar *fmt, PropertyName name )
    {
        AsmJSCompiler::OutputError(GetScriptContext(), fmt, name->Psz());
        return false;
    }

    bool AsmJsModuleCompiler::LookupStandardLibraryMathName( PropertyName name, MathBuiltin *mathBuiltin ) const
    {
        return mStandardLibraryMathNames.TryGetValue( name->GetPropertyId(), mathBuiltin );
    }

    bool AsmJsModuleCompiler::LookupStandardLibraryArrayName(PropertyName name, TypedArrayBuiltin *builtin) const
    {
        return mStandardLibraryArrayNames.TryGetValue(name->GetPropertyId(), builtin);
    }

    void AsmJsModuleCompiler::InitBufferArgName( PropertyName n )
    {
#if DBG
        Assert( !mBufferArgNameInit );
        mBufferArgNameInit = true;
#endif
        mBufferArgName = n;
    }

    void AsmJsModuleCompiler::InitForeignArgName( PropertyName n )
    {
#if DBG
        Assert( !mForeignArgNameInit );
        mForeignArgNameInit = true;
#endif
        mForeignArgName = n;
    }

    void AsmJsModuleCompiler::InitStdLibArgName( PropertyName n )
    {
#if DBG
        Assert( !mStdLibArgNameInit );
        mStdLibArgNameInit = true;
#endif
        mStdLibArgName = n;
    }

    Js::PropertyName AsmJsModuleCompiler::GetStdLibArgName() const
    {
#if DBG
        Assert( mBufferArgNameInit );
#endif
        return mStdLibArgName;
    }

    Js::PropertyName AsmJsModuleCompiler::GetForeignArgName() const
    {
#if DBG
        Assert( mForeignArgNameInit );
#endif
        return mForeignArgName;
    }

    Js::PropertyName AsmJsModuleCompiler::GetBufferArgName() const
    {
#if DBG
        Assert( mStdLibArgNameInit );
#endif
        return mBufferArgName;
    }

    bool AsmJsModuleCompiler::Init()
    {
        if( mInitialised )
        {
            return false;
        }
        mInitialised = true;

        struct MathFunc
        {
            MathFunc( PropertyId id_ = 0, AsmJsMathFunction* val_ = nullptr ) :
                id( id_ ), val( val_ )
            {
            }
            PropertyId id;
            AsmJsMathFunction* val;
        };
        MathFunc mathFunctions[AsmJSMathBuiltinFunction_COUNT];
        // we could move the mathBuiltinFuncname to MathFunc struct
        mathFunctions[AsmJSMathBuiltin_sin   ] = MathFunc(PropertyIds::sin   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_sin   , OpCodeAsmJs::Sin_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_cos   ] = MathFunc(PropertyIds::cos   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_cos   , OpCodeAsmJs::Cos_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_tan   ] = MathFunc(PropertyIds::tan   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_tan   , OpCodeAsmJs::Tan_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_asin  ] = MathFunc(PropertyIds::asin  , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_asin  , OpCodeAsmJs::Asin_Db  , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_acos  ] = MathFunc(PropertyIds::acos  , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_acos  , OpCodeAsmJs::Acos_Db  , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_atan  ] = MathFunc(PropertyIds::atan  , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_atan  , OpCodeAsmJs::Atan_Db  , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_ceil  ] = MathFunc(PropertyIds::ceil  , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_ceil  , OpCodeAsmJs::Ceil_Db  , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_floor ] = MathFunc(PropertyIds::floor , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_floor , OpCodeAsmJs::Floor_Db , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_exp   ] = MathFunc(PropertyIds::exp   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_exp   , OpCodeAsmJs::Exp_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_log   ] = MathFunc(PropertyIds::log   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_log   , OpCodeAsmJs::Log_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_pow   ] = MathFunc(PropertyIds::pow   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_pow   , OpCodeAsmJs::Pow_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble, AsmJsType::MaybeDouble ));
        mathFunctions[AsmJSMathBuiltin_sqrt  ] = MathFunc(PropertyIds::sqrt  , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_sqrt  , OpCodeAsmJs::Sqrt_Db  , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_abs   ] = MathFunc(PropertyIds::abs   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_abs   , OpCodeAsmJs::Abs_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_atan2 ] = MathFunc(PropertyIds::atan2 , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_atan2 , OpCodeAsmJs::Atan2_Db , AsmJsRetType::Double, AsmJsType::MaybeDouble, AsmJsType::MaybeDouble ));
        mathFunctions[AsmJSMathBuiltin_imul  ] = MathFunc(PropertyIds::imul  , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_imul  , OpCodeAsmJs::Imul_Int , AsmJsRetType::Signed, AsmJsType::Intish     , AsmJsType::Intish      ));
        mathFunctions[AsmJSMathBuiltin_fround] = MathFunc(PropertyIds::fround, Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_fround, OpCodeAsmJs::Fround_Flt,AsmJsRetType::Float , AsmJsType::Floatish                            ));
        mathFunctions[AsmJSMathBuiltin_min   ] = MathFunc(PropertyIds::min   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_min   , OpCodeAsmJs::Min_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble, AsmJsType::MaybeDouble));
        mathFunctions[AsmJSMathBuiltin_max   ] = MathFunc(PropertyIds::max   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_max   , OpCodeAsmJs::Max_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble, AsmJsType::MaybeDouble));
        mathFunctions[AsmJSMathBuiltin_clz32 ] = MathFunc(PropertyIds::clz32 , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_clz32 , OpCodeAsmJs::Clz32_Int, AsmJsRetType::Fixnum, AsmJsType::Intish));

        mathFunctions[AsmJSMathBuiltin_abs].val->SetOverload(Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_abs, OpCodeAsmJs::Abs_Int, AsmJsRetType::Unsigned, AsmJsType::Signed));
        mathFunctions[AsmJSMathBuiltin_min].val->SetOverload(Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_min, OpCodeAsmJs::Min_Int, AsmJsRetType::Signed,  AsmJsType::Signed,  AsmJsType::Signed));
        mathFunctions[AsmJSMathBuiltin_max].val->SetOverload(Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_max, OpCodeAsmJs::Max_Int, AsmJsRetType::Signed,  AsmJsType::Signed,  AsmJsType::Signed));

        //Float Overloads
        mathFunctions[AsmJSMathBuiltin_fround].val->SetOverload(Anew(&mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_fround, OpCodeAsmJs::Fround_Db,  AsmJsRetType::Float, AsmJsType::MaybeDouble));
        mathFunctions[AsmJSMathBuiltin_fround].val->SetOverload(Anew(&mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_fround, OpCodeAsmJs::Fround_Int, AsmJsRetType::Float, AsmJsType::Int));// should we split this into signed and unsigned?
        mathFunctions[AsmJSMathBuiltin_abs].val->SetOverload(   Anew(&mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_abs,    OpCodeAsmJs::Abs_Flt,    AsmJsRetType::Floatish, AsmJsType::MaybeFloat));
        mathFunctions[AsmJSMathBuiltin_ceil].val->SetOverload(  Anew(&mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_ceil,   OpCodeAsmJs::Ceil_Flt,   AsmJsRetType::Floatish, AsmJsType::MaybeFloat));
        mathFunctions[AsmJSMathBuiltin_floor].val->SetOverload( Anew(&mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_floor,  OpCodeAsmJs::Floor_Flt,  AsmJsRetType::Floatish, AsmJsType::MaybeFloat));
        mathFunctions[AsmJSMathBuiltin_sqrt].val->SetOverload(  Anew(&mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_sqrt,   OpCodeAsmJs::Sqrt_Flt,   AsmJsRetType::Floatish, AsmJsType::MaybeFloat));

        for (int i = 0; i < AsmJSMathBuiltinFunction_COUNT ; i++)
        {
            if( !AddStandardLibraryMathName( (PropertyId)mathFunctions[i].id, mathFunctions[i].val, mathFunctions[i].val->GetMathBuiltInFunction() ) )
            {
                return false;
            }
        }

        struct ConstMath
        {
            ConstMath( PropertyId id_, const double* val_, AsmJSMathBuiltinFunction mathLibConstName_):
                id(id_), val(val_), mathLibConstName(mathLibConstName_) { }
            PropertyId id;
            AsmJSMathBuiltinFunction mathLibConstName;
            const double* val;
        };
        ConstMath constMath[] = {
            ConstMath( PropertyIds::E       , &Math::E                           , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_e ),
            ConstMath(PropertyIds::LN10     , &Math::LN10                        , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ln10),
            ConstMath(PropertyIds::LN2      , &Math::LN2                         , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ln2),
            ConstMath(PropertyIds::LOG2E    , &Math::LOG2E                       , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log2e),
            ConstMath(PropertyIds::LOG10E   , &Math::LOG10E                      , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log10e),
            ConstMath(PropertyIds::PI       , &Math::PI                          , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_pi),
            ConstMath(PropertyIds::SQRT1_2  , &Math::SQRT1_2                     , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt1_2),
            ConstMath(PropertyIds::SQRT2    , &Math::SQRT2                       , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt2),
            ConstMath(PropertyIds::Infinity , &NumberConstants::POSITIVE_INFINITY, AsmJSMathBuiltinFunction::AsmJSMathBuiltin_infinity),
            ConstMath(PropertyIds::NaN      , &NumberConstants::NaN              , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_nan),
        };
        const int size = sizeof( constMath ) / sizeof( ConstMath );
        for (int i = 0; i < size ; i++)
        {
            if( !AddStandardLibraryMathName( constMath[i].id, constMath[i].val, constMath[i].mathLibConstName ) )
            {
                return false;
            }
        }


        struct ArrayFunc
        {
            ArrayFunc(PropertyId id_ = 0, AsmJsTypedArrayFunction* val_ = nullptr) :
                id(id_), val(val_)
            {
            }
            PropertyId id;
            AsmJsTypedArrayFunction* val;
        };

        ArrayFunc arrayFunctions[AsmJSMathBuiltinFunction_COUNT];
        arrayFunctions[AsmJSTypedArrayBuiltin_Int8Array   ] = ArrayFunc(PropertyIds::Int8Array,    Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Int8Array,    ArrayBufferView::TYPE_INT8));
        arrayFunctions[AsmJSTypedArrayBuiltin_Uint8Array  ] = ArrayFunc(PropertyIds::Uint8Array,   Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Uint8Array,   ArrayBufferView::TYPE_UINT8));
        arrayFunctions[AsmJSTypedArrayBuiltin_Int16Array  ] = ArrayFunc(PropertyIds::Int16Array,   Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Int16Array,   ArrayBufferView::TYPE_INT16));
        arrayFunctions[AsmJSTypedArrayBuiltin_Uint16Array ] = ArrayFunc(PropertyIds::Uint16Array,  Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Uint16Array,  ArrayBufferView::TYPE_UINT16));
        arrayFunctions[AsmJSTypedArrayBuiltin_Int32Array  ] = ArrayFunc(PropertyIds::Int32Array,   Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Int32Array,   ArrayBufferView::TYPE_INT32));
        arrayFunctions[AsmJSTypedArrayBuiltin_Uint32Array ] = ArrayFunc(PropertyIds::Uint32Array,  Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Uint32Array,  ArrayBufferView::TYPE_UINT32));
        arrayFunctions[AsmJSTypedArrayBuiltin_Float32Array] = ArrayFunc(PropertyIds::Float32Array, Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Float32Array, ArrayBufferView::TYPE_FLOAT32));
        arrayFunctions[AsmJSTypedArrayBuiltin_Float64Array] = ArrayFunc(PropertyIds::Float64Array, Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Float64Array, ArrayBufferView::TYPE_FLOAT64));
        arrayFunctions[AsmJSTypedArrayBuiltin_byteLength  ] = ArrayFunc(PropertyIds::byteLength,   Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_byteLength,   ArrayBufferView::TYPE_INVALID));

        for (int i = 0; i < AsmJSTypedArrayBuiltin_COUNT; i++)
        {
            if (!AddStandardLibraryArrayName((PropertyId)arrayFunctions[i].id, arrayFunctions[i].val, arrayFunctions[i].val->GetArrayBuiltInFunction()))
            {
                return false;
            }
        }
        // similar to math functions maps initialization.
        if (IsSimdjsEnabled())
        {
            if (!InitSIMDBuiltins())
            {
                return false;
            }
        }
        return true;
    }

    bool AsmJsModuleCompiler::InitSIMDBuiltins()
    {
        struct SIMDFunc
        {
            SIMDFunc(PropertyId id_ = 0, AsmJsSIMDFunction* val_ = nullptr) :
            id(id_), val(val_)
            {
            }
            PropertyId id;
            AsmJsSIMDFunction* val;
        };

        SIMDFunc simdFunctions[AsmJsSIMDBuiltin_COUNT];

        // !! NOTE: Keep these grouped by SIMD type

        /* Int32x4 builtins*/
        //-------------------
        simdFunctions[AsmJsSIMDBuiltin_Int32x4]                     = SIMDFunc(PropertyIds::Int32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 4, AsmJsSIMDBuiltin_Int32x4, OpCodeAsmJs::Simd128_IntsToI4, AsmJsRetType::Int32x4, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_check]               = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_check, OpCodeAsmJs::Simd128_Ld_I4 /*no dynamic checks*/, AsmJsRetType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_splat]               = SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_splat, OpCodeAsmJs::Simd128_Splat_I4, AsmJsRetType::Int32x4, AsmJsType::Int));
        // Q: Is this operation supported in ASMJS ? We don't have bool type.
        //simdFunctions[AsmJsSIMDBuiltin_int32x4_bool]                = SIMDFunc(PropertyIds::bool_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 4, AsmJsSIMDBuiltin_int32x4_bool, OpCodeAsmJs::Simd128_Bool_I4, AsmJsRetType::Int32x4, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromFloat64x2]       = SIMDFunc(PropertyIds::fromFloat64x2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromFloat64x2, OpCodeAsmJs::Simd128_FromFloat64x2_I4, AsmJsRetType::Int32x4, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromFloat64x2Bits]   = SIMDFunc(PropertyIds::fromFloat64x2Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromFloat64x2Bits, OpCodeAsmJs::Simd128_FromFloat64x2Bits_I4, AsmJsRetType::Int32x4, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromFloat32x4]       = SIMDFunc(PropertyIds::fromFloat32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromFloat32x4, OpCodeAsmJs::Simd128_FromFloat32x4_I4, AsmJsRetType::Int32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromFloat32x4Bits]   = SIMDFunc(PropertyIds::fromFloat32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromFloat32x4Bits, OpCodeAsmJs::Simd128_FromFloat32x4Bits_I4, AsmJsRetType::Int32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_neg]                 = SIMDFunc(PropertyIds::neg, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_neg, OpCodeAsmJs::Simd128_Neg_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_add]                 = SIMDFunc(PropertyIds::add, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_add, OpCodeAsmJs::Simd128_Add_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_sub]                 = SIMDFunc(PropertyIds::sub, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_sub, OpCodeAsmJs::Simd128_Sub_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_mul]                 = SIMDFunc(PropertyIds::mul, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_mul, OpCodeAsmJs::Simd128_Mul_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        // TODO: Enable after fix in lib
        simdFunctions[AsmJsSIMDBuiltin_int32x4_swizzle]             = SIMDFunc(PropertyIds::swizzle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 5, AsmJsSIMDBuiltin_int32x4_swizzle, OpCodeAsmJs::Simd128_Swizzle_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_shuffle]             = SIMDFunc(PropertyIds::shuffle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 6, AsmJsSIMDBuiltin_int32x4_shuffle, OpCodeAsmJs::Simd128_Shuffle_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_extractLane]         = SIMDFunc(PropertyIds::extractLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_extractLane, OpCodeAsmJs::Simd128_ExtractLane_I4, AsmJsRetType::Signed, AsmJsType::Int32x4, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_replaceLane]         = SIMDFunc(PropertyIds::replaceLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int32x4_replaceLane, OpCodeAsmJs::Simd128_ReplaceLane_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_lessThan]            = SIMDFunc(PropertyIds::lessThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_lessThan, OpCodeAsmJs::Simd128_Lt_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_equal]               = SIMDFunc(PropertyIds::equal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_equal, OpCodeAsmJs::Simd128_Eq_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_greaterThan]         = SIMDFunc(PropertyIds::greaterThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_greaterThan, OpCodeAsmJs::Simd128_Gt_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_select]              = SIMDFunc(PropertyIds::select, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int32x4_select, OpCodeAsmJs::Simd128_Select_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_and]                 = SIMDFunc(PropertyIds::and, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_and, OpCodeAsmJs::Simd128_And_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_or]                  = SIMDFunc(PropertyIds::or, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_or, OpCodeAsmJs::Simd128_Or_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_xor]                 = SIMDFunc(PropertyIds::xor, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_xor, OpCodeAsmJs::Simd128_Xor_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_not]                 = SIMDFunc(PropertyIds::not, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_not, OpCodeAsmJs::Simd128_Not_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4));

        // Loads and Stores
        // We fill Void for the tarray type. This is ok since we special handle these ops.
        simdFunctions[AsmJsSIMDBuiltin_int32x4_load]                = SIMDFunc(PropertyIds::load, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_load, OpCodeAsmJs::Simd128_LdArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_load1]               = SIMDFunc(PropertyIds::load1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_load1, OpCodeAsmJs::Simd128_LdArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_load2]               = SIMDFunc(PropertyIds::load2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_load2, OpCodeAsmJs::Simd128_LdArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_load3]               = SIMDFunc(PropertyIds::load3, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_load3, OpCodeAsmJs::Simd128_LdArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_store]               = SIMDFunc(PropertyIds::store, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int32x4_store, OpCodeAsmJs::Simd128_StArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_store1]              = SIMDFunc(PropertyIds::store1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int32x4_store1, OpCodeAsmJs::Simd128_StArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_store2]              = SIMDFunc(PropertyIds::store2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int32x4_store2, OpCodeAsmJs::Simd128_StArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_store3]              = SIMDFunc(PropertyIds::store3, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int32x4_store3, OpCodeAsmJs::Simd128_StArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Int32x4));

        /* Float32x4 builtins*/
        //-------------------
        simdFunctions[AsmJsSIMDBuiltin_Float32x4]                   = SIMDFunc(PropertyIds::Float32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 4, AsmJsSIMDBuiltin_Float32x4, OpCodeAsmJs::Simd128_FloatsToF4, AsmJsRetType::Float32x4, AsmJsType::FloatishDoubleLit, AsmJsType::FloatishDoubleLit, AsmJsType::FloatishDoubleLit, AsmJsType::FloatishDoubleLit));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_check]             = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_check, OpCodeAsmJs::Simd128_Ld_F4 /*no dynamic checks*/, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_splat]             = SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_splat, OpCodeAsmJs::Simd128_Splat_F4, AsmJsRetType::Float32x4, AsmJsType::FloatishDoubleLit));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromFloat64x2]     = SIMDFunc(PropertyIds::fromFloat64x2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromFloat64x2, OpCodeAsmJs::Simd128_FromFloat64x2_F4, AsmJsRetType::Float32x4, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromFloat64x2Bits] = SIMDFunc(PropertyIds::fromFloat64x2Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromFloat64x2Bits, OpCodeAsmJs::Simd128_FromFloat64x2Bits_F4, AsmJsRetType::Float32x4, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromInt32x4]       = SIMDFunc(PropertyIds::fromInt32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromInt32x4, OpCodeAsmJs::Simd128_FromInt32x4_F4, AsmJsRetType::Float32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromInt32x4Bits]   = SIMDFunc(PropertyIds::fromInt32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromInt32x4Bits, OpCodeAsmJs::Simd128_FromInt32x4Bits_F4, AsmJsRetType::Float32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_abs]               = SIMDFunc(PropertyIds::abs, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_abs, OpCodeAsmJs::Simd128_Abs_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_neg]               = SIMDFunc(PropertyIds::neg, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_neg, OpCodeAsmJs::Simd128_Neg_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_add]               = SIMDFunc(PropertyIds::add, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_add, OpCodeAsmJs::Simd128_Add_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_sub]               = SIMDFunc(PropertyIds::sub, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_sub, OpCodeAsmJs::Simd128_Sub_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_mul]               = SIMDFunc(PropertyIds::mul, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_mul, OpCodeAsmJs::Simd128_Mul_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_div]               = SIMDFunc(PropertyIds::div, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_div, OpCodeAsmJs::Simd128_Div_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_clamp]             = SIMDFunc(PropertyIds::clamp, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_clamp, OpCodeAsmJs::Simd128_Clamp_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_min]               = SIMDFunc(PropertyIds::min, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_min, OpCodeAsmJs::Simd128_Min_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_max]               = SIMDFunc(PropertyIds::max, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_max, OpCodeAsmJs::Simd128_Max_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_reciprocal]        = SIMDFunc(PropertyIds::reciprocal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_reciprocal, OpCodeAsmJs::Simd128_Rcp_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_reciprocalSqrt]    = SIMDFunc(PropertyIds::reciprocalSqrt, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_reciprocalSqrt, OpCodeAsmJs::Simd128_RcpSqrt_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_sqrt]              = SIMDFunc(PropertyIds::sqrt, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_sqrt, OpCodeAsmJs::Simd128_Sqrt_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_swizzle]           = SIMDFunc(PropertyIds::swizzle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 5, AsmJsSIMDBuiltin_float32x4_swizzle, OpCodeAsmJs::Simd128_Swizzle_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_shuffle]           = SIMDFunc(PropertyIds::shuffle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 6, AsmJsSIMDBuiltin_float32x4_shuffle, OpCodeAsmJs::Simd128_Shuffle_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_extractLane]       = SIMDFunc(PropertyIds::extractLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_extractLane, OpCodeAsmJs::Simd128_ExtractLane_F4, AsmJsRetType::Float, AsmJsType::Float32x4, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_replaceLane]       = SIMDFunc(PropertyIds::replaceLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_replaceLane, OpCodeAsmJs::Simd128_ReplaceLane_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Int, AsmJsType::FloatishDoubleLit));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_lessThan]          = SIMDFunc(PropertyIds::lessThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_lessThan, OpCodeAsmJs::Simd128_Lt_F4, AsmJsRetType::Int32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_lessThanOrEqual]   = SIMDFunc(PropertyIds::lessThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_lessThanOrEqual, OpCodeAsmJs::Simd128_LtEq_F4, AsmJsRetType::Int32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_equal]             = SIMDFunc(PropertyIds::equal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_equal, OpCodeAsmJs::Simd128_Eq_F4, AsmJsRetType::Int32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_notEqual]          = SIMDFunc(PropertyIds::notEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_notEqual, OpCodeAsmJs::Simd128_Neq_F4, AsmJsRetType::Int32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_greaterThan]       = SIMDFunc(PropertyIds::greaterThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_greaterThan, OpCodeAsmJs::Simd128_Gt_F4, AsmJsRetType::Int32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_greaterThanOrEqual]= SIMDFunc(PropertyIds::greaterThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_greaterThanOrEqual, OpCodeAsmJs::Simd128_GtEq_F4, AsmJsRetType::Int32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_select]            = SIMDFunc(PropertyIds::select, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_select, OpCodeAsmJs::Simd128_Select_F4, AsmJsRetType::Float32x4, AsmJsType::Int32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_and]               = SIMDFunc(PropertyIds::and, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_and, OpCodeAsmJs::Simd128_And_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_or]                = SIMDFunc(PropertyIds::or, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_or, OpCodeAsmJs::Simd128_Or_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_xor]               = SIMDFunc(PropertyIds::xor, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_xor, OpCodeAsmJs::Simd128_Xor_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_not]               = SIMDFunc(PropertyIds::not, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_not, OpCodeAsmJs::Simd128_Not_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_load]              = SIMDFunc(PropertyIds::load, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_load, OpCodeAsmJs::Simd128_LdArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_load1]             = SIMDFunc(PropertyIds::load1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_load1, OpCodeAsmJs::Simd128_LdArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_load2]             = SIMDFunc(PropertyIds::load2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_load2, OpCodeAsmJs::Simd128_LdArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_load3]             = SIMDFunc(PropertyIds::load3, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_load3, OpCodeAsmJs::Simd128_LdArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_store]             = SIMDFunc(PropertyIds::store, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_store, OpCodeAsmJs::Simd128_StArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_store1]            = SIMDFunc(PropertyIds::store1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_store1, OpCodeAsmJs::Simd128_StArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_store2]            = SIMDFunc(PropertyIds::store2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_store2, OpCodeAsmJs::Simd128_StArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_store3]            = SIMDFunc(PropertyIds::store3, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_store3, OpCodeAsmJs::Simd128_StArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Float32x4));

        /* Float64x2 builtins*/
        //-------------------
        simdFunctions[AsmJsSIMDBuiltin_Float64x2]                   = SIMDFunc(PropertyIds::Float64x2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_Float64x2, OpCodeAsmJs::Simd128_DoublesToD2, AsmJsRetType::Float64x2, AsmJsType::MaybeDouble, AsmJsType::MaybeDouble));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_check]             = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_check, OpCodeAsmJs::Simd128_Ld_D2 /*no dynamic checks*/, AsmJsRetType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_splat]             = SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_splat, OpCodeAsmJs::Simd128_Splat_D2, AsmJsRetType::Float64x2, AsmJsType::Double));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_fromFloat32x4]     = SIMDFunc(PropertyIds::fromFloat32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_fromFloat32x4, OpCodeAsmJs::Simd128_FromFloat32x4_D2, AsmJsRetType::Float64x2, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_fromFloat32x4Bits] = SIMDFunc(PropertyIds::fromFloat32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_fromFloat32x4Bits, OpCodeAsmJs::Simd128_FromFloat32x4Bits_D2, AsmJsRetType::Float64x2, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_fromInt32x4]       = SIMDFunc(PropertyIds::fromInt32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_fromInt32x4, OpCodeAsmJs::Simd128_FromInt32x4_D2, AsmJsRetType::Float64x2, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_fromInt32x4Bits]   = SIMDFunc(PropertyIds::fromInt32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_fromInt32x4Bits, OpCodeAsmJs::Simd128_FromInt32x4Bits_D2, AsmJsRetType::Float64x2, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_abs]               = SIMDFunc(PropertyIds::abs, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_abs, OpCodeAsmJs::Simd128_Abs_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_neg]               = SIMDFunc(PropertyIds::neg, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_neg, OpCodeAsmJs::Simd128_Neg_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_add]               = SIMDFunc(PropertyIds::add, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_add, OpCodeAsmJs::Simd128_Add_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_sub]               = SIMDFunc(PropertyIds::sub, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_sub, OpCodeAsmJs::Simd128_Sub_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_mul]               = SIMDFunc(PropertyIds::mul, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_mul, OpCodeAsmJs::Simd128_Mul_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_div]               = SIMDFunc(PropertyIds::div, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_div, OpCodeAsmJs::Simd128_Div_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_clamp]             = SIMDFunc(PropertyIds::clamp, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float64x2_clamp, OpCodeAsmJs::Simd128_Clamp_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_min]               = SIMDFunc(PropertyIds::min, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_min, OpCodeAsmJs::Simd128_Min_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_max]               = SIMDFunc(PropertyIds::max, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_max, OpCodeAsmJs::Simd128_Max_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_reciprocal]        = SIMDFunc(PropertyIds::reciprocal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1,  AsmJsSIMDBuiltin_float64x2_reciprocal, OpCodeAsmJs::Simd128_Rcp_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_reciprocalSqrt]    = SIMDFunc(PropertyIds::reciprocalSqrt, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_reciprocalSqrt, OpCodeAsmJs::Simd128_RcpSqrt_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_sqrt]              = SIMDFunc(PropertyIds::sqrt, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_sqrt, OpCodeAsmJs::Simd128_Sqrt_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_swizzle]           = SIMDFunc(PropertyIds::swizzle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float64x2_swizzle, OpCodeAsmJs::Simd128_Swizzle_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_shuffle]           = SIMDFunc(PropertyIds::shuffle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 4, AsmJsSIMDBuiltin_float64x2_shuffle, OpCodeAsmJs::Simd128_Shuffle_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_lessThan]          = SIMDFunc(PropertyIds::lessThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_lessThan, OpCodeAsmJs::Simd128_Lt_D2, AsmJsRetType::Int32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_lessThanOrEqual]   = SIMDFunc(PropertyIds::lessThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_lessThanOrEqual, OpCodeAsmJs::Simd128_LtEq_D2, AsmJsRetType::Int32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_equal]             = SIMDFunc(PropertyIds::equal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_equal, OpCodeAsmJs::Simd128_Eq_D2, AsmJsRetType::Int32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_notEqual]          = SIMDFunc(PropertyIds::notEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_notEqual, OpCodeAsmJs::Simd128_Neq_D2, AsmJsRetType::Int32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_greaterThan]       = SIMDFunc(PropertyIds::greaterThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_greaterThan, OpCodeAsmJs::Simd128_Gt_D2, AsmJsRetType::Int32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_greaterThanOrEqual]= SIMDFunc(PropertyIds::greaterThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_greaterThanOrEqual, OpCodeAsmJs::Simd128_GtEq_D2, AsmJsRetType::Int32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_select]            = SIMDFunc(PropertyIds::select, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float64x2_select, OpCodeAsmJs::Simd128_Select_D2, AsmJsRetType::Float64x2, AsmJsType::Int32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));

        simdFunctions[AsmJsSIMDBuiltin_float64x2_load]              = SIMDFunc(PropertyIds::load, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_load, OpCodeAsmJs::Simd128_LdArr_D2, AsmJsRetType::Float64x2, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_load1]             = SIMDFunc(PropertyIds::load1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_load1, OpCodeAsmJs::Simd128_LdArr_D2, AsmJsRetType::Float64x2, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_store]             = SIMDFunc(PropertyIds::store, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float64x2_store, OpCodeAsmJs::Simd128_StArr_D2, AsmJsRetType::Float64x2, AsmJsType::Void, AsmJsType::Int, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_store1]            = SIMDFunc(PropertyIds::store1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float64x2_store1, OpCodeAsmJs::Simd128_StArr_D2, AsmJsRetType::Float64x2, AsmJsType::Void, AsmJsType::Int, AsmJsType::Float64x2));


        {
            SIMDNameMap *map = &mStdLibSIMDInt32x4Map;
            for (int i = 0; i < AsmJsSIMDBuiltin_COUNT; i++)
            {
                if (i == AsmJsSIMDBuiltin_Float32x4)
                {
                    map = &mStdLibSIMDFloat32x4Map;
                }
                if (i == AsmJsSIMDBuiltin_Float64x2)
                {
                    map = &mStdLibSIMDFloat64x2Map;
                }
                if (simdFunctions[i].id && simdFunctions[i].val)
                {
                    if (!AddStandardLibrarySIMDNameInMap(simdFunctions[i].id, simdFunctions[i].val, map))
                    {
                        AsmJSCompiler::OutputError(GetScriptContext(), L"Cannot initialize SIMD library");
                        return false;
                    }
                }
            }
        }
        return true;
    }

    AsmJsModuleCompiler::AsmJsModuleCompiler( ExclusiveContext *cx, AsmJSParser &parser ) :
        mCx( cx )
        , mCurrentParserNode( parser )
        , mAllocator( L"Asmjs", cx->scriptContext->GetThreadContext()->GetPageAllocator(), Throw::OutOfMemory )
        , mModuleFunctionName( nullptr )
        , mStandardLibraryMathNames(&mAllocator)
        , mStandardLibraryArrayNames(&mAllocator)
        , mFunctionArray( &mAllocator )
        , mModuleEnvironment( &mAllocator )
        , mFunctionTableArray( &mAllocator )
        , mInitialised(false)
        , mIntVarSpace( )
        , mDoubleVarSpace( )
        , mExports(&mAllocator)
        , mExportFuncIndex(Js::Constants::NoRegister)
        , mVarImportCount(0)
        , mVarCount(0)
        , mFuncPtrTableCount(0)
        , mCompileTime()
        , mCompileTimeLastTick(GetTick())
        , mMaxAstSize(0)
        , mArrayViews(&mAllocator)
        , mUsesChangeHeap(false)
        , mUsesHeapBuffer(false)
        , mMaxHeapAccess(0)
#if DBG
        , mStdLibArgNameInit(false)
        , mForeignArgNameInit(false)
        , mBufferArgNameInit(false)
#endif
        , mStdLibSIMDInt32x4Map(&mAllocator)
        , mStdLibSIMDFloat32x4Map(&mAllocator)
        , mStdLibSIMDFloat64x2Map(&mAllocator)
    {
        InitModuleNode( parser );
    }

    bool AsmJsModuleCompiler::AddStandardLibraryMathName( PropertyId id, const double* cstAddr, AsmJSMathBuiltinFunction mathLibFunctionName )
    {
        // make sure this name is unique
        if( mStandardLibraryMathNames.ContainsKey( id ) )
        {
            return false;
        }

        MathBuiltin mathBuiltin(mathLibFunctionName, cstAddr);
        int addResult = mStandardLibraryMathNames.AddNew( id, mathBuiltin );
        if( addResult == -1 )
        {
            // Error adding the function
            return false;
        }
        return true;
    }


    bool AsmJsModuleCompiler::AddStandardLibraryMathName(PropertyId id, AsmJsMathFunction* func, AsmJSMathBuiltinFunction mathLibFunctionName)
    {
        // make sure this name is unique
        if( mStandardLibraryMathNames.ContainsKey( id ) )
        {
            return false;
        }

        MathBuiltin mathBuiltin(mathLibFunctionName, func);
        int addResult = mStandardLibraryMathNames.AddNew( id, mathBuiltin );
        if( addResult == -1 )
        {
            // Error adding the function
            return false;
        }
        return true;
    }

    bool AsmJsModuleCompiler::AddStandardLibraryArrayName(PropertyId id, AsmJsTypedArrayFunction* func, AsmJSTypedArrayBuiltinFunction arrayLibFunctionName)
    {
        // make sure this name is unique
        if (mStandardLibraryArrayNames.ContainsKey(id))
        {
            return false;
        }

        TypedArrayBuiltin arrayBuiltin(arrayLibFunctionName, func);
        int addResult = mStandardLibraryArrayNames.AddNew(id, arrayBuiltin);
        if (addResult == -1)
        {
            // Error adding the function
            return false;
        }
        return true;
    }

    Parser * AsmJsModuleCompiler::GetParser() const
    {
        return mCx->byteCodeGenerator->GetParser();
    }

    ByteCodeGenerator* AsmJsModuleCompiler::GetByteCodeGenerator() const
    {
        return mCx->byteCodeGenerator;
    }

    ScriptContext * AsmJsModuleCompiler::GetScriptContext() const
    {
        return mCx->scriptContext;
    }

    AsmJsSymbol* AsmJsModuleCompiler::LookupIdentifier( PropertyName name, AsmJsFunc* func /*= nullptr */, AsmJsLookupSource::Source* lookupSource /*= nullptr*/ )
    {
        AsmJsSymbol* lookupResult = nullptr;
        if (name)
        {
            if (func)
            {
                lookupResult = func->LookupIdentifier(name, lookupSource);
                if (lookupResult)
                {
                    return lookupResult;
                }
            }

            lookupResult = mModuleEnvironment.LookupWithKey(name->GetPropertyId(), nullptr);
            if (lookupSource)
            {
                *lookupSource = AsmJsLookupSource::AsmJsModule;
            }
        }
        return lookupResult;
    }

    bool AsmJsModuleCompiler::DefineIdentifier( PropertyName name, AsmJsSymbol* symbol )
    {
        Assert( symbol );
        if( symbol )
        {
            // make sure this identifier is unique
            if(!LookupIdentifier( name ))
            {
                int addResult = mModuleEnvironment.AddNew(name->GetPropertyId(), symbol);
                return addResult != -1;
            }
        }
        return false;
    }

    bool AsmJsModuleCompiler::AddNumericVar( PropertyName name, ParseNode* pnode, bool isFloat, bool isMutable /*= true*/ )
    {
        Assert(ParserWrapper::IsNumericLiteral(pnode) || (isFloat && ParserWrapper::IsFroundNumericLiteral(pnode)));
        AsmJsVar* var = Anew( &mAllocator, AsmJsVar, name, isMutable );
        if( !var )
        {
            return false;
        }
        if( !DefineIdentifier( name, var ) )
        {
            return false;
        }

        ++mVarCount;

        if (isFloat)
        {
            var->SetVarType(AsmJsVarType::Float);
            var->SetLocation(mFloatVarSpace.AcquireRegister());
            if (pnode->nop == knopInt)
            {
                var->SetConstInitialiser((float)pnode->sxInt.lw);
            }
            else if (ParserWrapper::IsNegativeZero(pnode))
            {
                var->SetConstInitialiser(-0.0f);
            }
            else
            {
                var->SetConstInitialiser((float)pnode->sxFlt.dbl);
            }
        }
        else if (pnode->nop == knopInt)
        {
            var->SetVarType(AsmJsVarType::Int);
            var->SetLocation(mIntVarSpace.AcquireRegister());
            var->SetConstInitialiser(pnode->sxInt.lw);
        }
        else
        {
            if (ParserWrapper::IsMinInt(pnode))
            {
                var->SetVarType(AsmJsVarType::Int);
                var->SetLocation(mIntVarSpace.AcquireRegister());
                var->SetConstInitialiser(MININT);
            }
            else if (ParserWrapper::IsUnsigned(pnode))
            {
                var->SetVarType(AsmJsVarType::Int);
                var->SetLocation(mIntVarSpace.AcquireRegister());
                var->SetConstInitialiser((int)((uint32)pnode->sxFlt.dbl));
            }
            else if (pnode->sxFlt.maybeInt)
            {
                // this means there was an int literal not in range [-2^31,3^32)
                return false;
            }
            else
            {
                var->SetVarType(AsmJsVarType::Double);
                var->SetLocation(mDoubleVarSpace.AcquireRegister());
                var->SetConstInitialiser(pnode->sxFlt.dbl);
            }
        }
        return true;
    }

    bool AsmJsModuleCompiler::AddGlobalVarImport( PropertyName name, PropertyName field, AsmJSCoercion coercion )
    {
        AsmJsConstantImport* var = Anew( &mAllocator, AsmJsConstantImport, name, field );
        if( !var )
        {
            return false;
        }
        if( !DefineIdentifier( name, var ) )
        {
            return false;
        }
        ++mVarImportCount;

        switch( coercion )
        {
        case Js::AsmJS_ToInt32:
            var->SetVarType( AsmJsVarType::Int );
            var->SetLocation( mIntVarSpace.AcquireRegister() );
            break;
        case Js::AsmJS_ToNumber:
            var->SetVarType( AsmJsVarType::Double );
            var->SetLocation( mDoubleVarSpace.AcquireRegister() );
            break;
        case Js::AsmJS_FRound:
            var->SetVarType( AsmJsVarType::Float );
            var->SetLocation(mFloatVarSpace.AcquireRegister());
            break;
        case Js::AsmJS_Int32x4:
            if (IsSimdjsEnabled())
            {
                var->SetVarType(AsmJsVarType::Int32x4);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        case AsmJS_Float32x4:
            if (IsSimdjsEnabled())
            {
                var->SetVarType(AsmJsVarType::Float32x4);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        case AsmJS_Float64x2:
            if (IsSimdjsEnabled())
            {
                var->SetVarType(AsmJsVarType::Float64x2);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        default:
            break;
        }

        return true;
    }

    bool AsmJsModuleCompiler::AddModuleFunctionImport( PropertyName name, PropertyName field )
    {
        AsmJsImportFunction* var = Anew( &mAllocator, AsmJsImportFunction, name, field, &mAllocator );
        if( !var )
        {
            return false;
        }
        if( !DefineIdentifier( name, var ) )
        {
            return false;
        }
        var->SetFunctionIndex( mImportFunctions.AcquireRegister() );

        return true;
    }

    bool AsmJsModuleCompiler::AddNumericConst( PropertyName name, const double* cst )
    {
        AsmJsMathConst* var = Anew( &mAllocator, AsmJsMathConst, name, cst );
        if( !var )
        {
            return false;
        }
        if( !DefineIdentifier( name, var ) )
        {
            return false;
        }

        return true;
    }

    bool AsmJsModuleCompiler::AddArrayView( PropertyName name, ArrayBufferView::ViewType type )
    {
        AsmJsArrayView* view = Anew( &mAllocator, AsmJsArrayView, name, type );
        if( !view )
        {
            return false;
        }
        if( !DefineIdentifier( name, view ) )
        {
            return false;
        }
        mArrayViews.Enqueue(view);

        return true;
    }

    bool AsmJsModuleCompiler::AddFunctionTable( PropertyName name, const int size )
    {
        GetByteCodeGenerator()->AssignPropertyId(name);
        AsmJsFunctionTable* funcTable = Anew( &mAllocator, AsmJsFunctionTable, name, &mAllocator );
        if( !funcTable )
        {
            return false;
        }
        if( !DefineIdentifier( name, funcTable ) )
        {
            return false;
        }
        funcTable->SetSize( size );
        int pos = mFunctionTableArray.Add( funcTable );
        funcTable->SetFunctionIndex( pos );

        return true;
    }

    bool AsmJsModuleCompiler::AddExport( PropertyName name, RegSlot location )
    {
        AsmJsModuleExport ex;
        ex.id = name->GetPropertyId();
        ex.location = location;

        // return is < 0 if count overflowed 31bits
        return mExports.Add( ex ) >= 0;
    }

    bool AsmJsModuleCompiler::SetExportFunc( AsmJsFunc* func )
    {
        Assert( mExports.Count() == 0 && func);
        mExportFuncIndex = func->GetFunctionIndex();
        return mExports.Count() == 0 && (uint32)mExportFuncIndex < (uint32)mFunctionArray.Count();
    }

    AsmJsFunctionDeclaration* AsmJsModuleCompiler::LookupFunction( PropertyName name )
    {
        if (name)
        {
            AsmJsSymbol* sym = LookupIdentifier(name);
            if (sym)
            {
                switch (sym->GetSymbolType())
                {
                case AsmJsSymbol::SIMDBuiltinFunction:
                case AsmJsSymbol::MathBuiltinFunction:
                case AsmJsSymbol::ModuleFunction:
                case AsmJsSymbol::ImportFunction:
                case AsmJsSymbol::FuncPtrTable:
                    return sym->Cast<AsmJsFunctionDeclaration>();
                default:
                    break;
                }
            }
        }
        return nullptr;
    }

    bool AsmJsModuleCompiler::AreAllFuncTableDefined()
    {
        const int size = mFunctionTableArray.Count();
        for (int i = 0; i < size ; i++)
        {
            AsmJsFunctionTable* funcTable = mFunctionTableArray.Item( i );
            if( !funcTable->IsDefined() )
            {
                AsmJSCompiler::OutputError(GetScriptContext(), L"Function table %s was used in a function but does not appear in the module", funcTable->GetName()->Psz());
                return false;
            }
        }
        return true;
    }

    void AsmJsModuleCompiler::UpdateMaxHeapAccess(uint index)
    {
        if (mMaxHeapAccess < index)
        {
            mMaxHeapAccess = index;
        }
    }

    void AsmJsModuleCompiler::InitMemoryOffsets()
    {
        mModuleMemory.mArrayBufferOffset = AsmJsModuleMemory::MemoryTableBeginOffset;
        mModuleMemory.mStdLibOffset = mModuleMemory.mArrayBufferOffset + 1;
        mModuleMemory.mDoubleOffset = mModuleMemory.mStdLibOffset + 1;
        mModuleMemory.mFuncOffset = mModuleMemory.mDoubleOffset + (mDoubleVarSpace.GetTotalVarCount() * DOUBLE_SLOTS_SPACE);
        mModuleMemory.mFFIOffset = mModuleMemory.mFuncOffset + mFunctionArray.Count();
        mModuleMemory.mFuncPtrOffset = mModuleMemory.mFFIOffset + mImportFunctions.GetTotalVarCount();
        mModuleMemory.mFloatOffset = mModuleMemory.mFuncPtrOffset + GetFuncPtrTableCount();
        mModuleMemory.mIntOffset = mModuleMemory.mFloatOffset + (int32)(mFloatVarSpace.GetTotalVarCount() * FLOAT_SLOTS_SPACE + 0.5);
        mModuleMemory.mMemorySize    = mModuleMemory.mIntOffset + (int32)(mIntVarSpace.GetTotalVarCount() * INT_SLOTS_SPACE + 0.5);

        if (IsSimdjsEnabled())
        {
            mModuleMemory.mSimdOffset = (int) ::ceil(mModuleMemory.mMemorySize / SIMD_SLOTS_SPACE);
            if (mSimdVarSpace.GetTotalVarCount())
            {
                mModuleMemory.mMemorySize = (int)((mModuleMemory.mSimdOffset + mSimdVarSpace.GetTotalVarCount()) * SIMD_SLOTS_SPACE);
                // no alignment
                // mModuleMemory.mMemorySize += (int)SIMD_SLOTS_SPACE;
            }
        }
    }

    void AsmJsModuleCompiler::AccumulateCompileTime()
    {
        Js::TickDelta td;
        AsmJsCompileTime curTime = GetTick();
        td = curTime - mCompileTimeLastTick;
        mCompileTime = mCompileTime+td;
        mCompileTimeLastTick = curTime;
    }

    void AsmJsModuleCompiler::AccumulateCompileTime(AsmJsCompilation::Phases phase)
    {
        Js::TickDelta td;
        AsmJsCompileTime curTime = GetTick();
        td = curTime - mCompileTimeLastTick;
        mCompileTime = mCompileTime+td;
        mCompileTimeLastTick = curTime;
        mPhaseCompileTime[phase] = mPhaseCompileTime[phase] + td;
    }

    Js::AsmJsCompileTime AsmJsModuleCompiler::GetTick()
    {
        return Js::Tick::Now();
    }

    uint64 AsmJsModuleCompiler::GetCompileTime() const
{
        return mCompileTime.ToMicroseconds();
    }

    static const wchar_t* AsmPhaseNames[AsmJsCompilation::Phases_COUNT] = {
        L"Module",
        L"ByteCode",
        L"TemplateJIT",
    };

    void AsmJsModuleCompiler::PrintCompileTrace() const
    {
        // for testtrace, don't print time so that it can be used for baselines
        if (PHASE_TESTTRACE1(AsmjsPhase))
        {
            AsmJSCompiler::OutputMessage(GetScriptContext(), DEIT_ASMJS_SUCCEEDED, L"Successfully compiled asm.js code");
        }
        else
        {
            uint64 us = GetCompileTime();
            uint64 ms = us / 1000;
            us = us % 1000;
            AsmJSCompiler::OutputMessage(GetScriptContext(), DEIT_ASMJS_SUCCEEDED, L"Successfully compiled asm.js code (total compilation time %llu.%llums)", ms, us);
        }

        if (PHASE_TRACE1(AsmjsPhase))
        {
            for (int i = 0; i < AsmJsCompilation::Phases_COUNT; i++)
            {
                uint64 us = mPhaseCompileTime[i].ToMicroseconds();
                uint64 ms = us / 1000;
                us = us % 1000;
                Output::Print(L"%20s : %llu.%llums\n", AsmPhaseNames[i], ms, us);
            }
            Output::Flush();
        }
    }

    BVStatic<ASMMATH_BUILTIN_SIZE> AsmJsModuleCompiler::GetAsmMathBuiltinUsedBV()
    {
        return mAsmMathBuiltinUsedBV;
    }

    BVStatic<ASMARRAY_BUILTIN_SIZE> AsmJsModuleCompiler::GetAsmArrayBuiltinUsedBV()
    {
        return mAsmArrayBuiltinUsedBV;
    }

    void AsmJsModuleInfo::SetFunctionCount( int val )
    {
        Assert( mFunctions == nullptr );
        mFunctionCount = val;
        mFunctions = RecyclerNewArray( mRecycler, ModuleFunction, val );
    }

    void AsmJsModuleInfo::SetFunctionTableCount( int val )
    {
        Assert( mFunctionTables == nullptr );
        mFunctionTableCount = val;
        mFunctionTables = RecyclerNewArray( mRecycler, ModuleFunctionTable, val );
    }

    void AsmJsModuleInfo::SetFunctionImportCount( int val )
    {
        Assert( mFunctionImports == nullptr );
        mFunctionImportCount = val;
        mFunctionImports = RecyclerNewArray( mRecycler, ModuleFunctionImport, val );
    }

    void AsmJsModuleInfo::SetVarCount( int val )
    {
        Assert( mVars == nullptr );
        mVarCount = val;
        mVars = RecyclerNewArray( mRecycler, ModuleVar, val );
    }

    void AsmJsModuleInfo::SetVarImportCount( int val )
    {
        Assert( mVarImports == nullptr );
        mVarImportCount = val;
        mVarImports = RecyclerNewArray( mRecycler, ModuleVarImport, val );
    }

    void AsmJsModuleInfo::SetExportsCount( int count )
    {
        if( count )
        {
            mExports = RecyclerNewPlus( mRecycler, count * sizeof( PropertyId ), PropertyIdArray, count );
            mExportsFunctionLocation = RecyclerNewArray( mRecycler, RegSlot, count );
        }
        mExportsCount = count;
    }

    void AsmJsModuleInfo::InitializeSlotMap(int val)
    {
        Assert(mSlotMap == nullptr);
        mSlotsCount = val;
        mSlotMap = RecyclerNew(mRecycler, AsmJsSlotMap, mRecycler);
    }

    void AsmJsModuleInfo::SetFunctionTableSize( int index, uint size )
    {
        Assert( mFunctionTables != nullptr );
        Assert( index < mFunctionTableCount );
        ModuleFunctionTable& table = mFunctionTables[index];
        table.size = size;
        table.moduleFunctionIndex = RecyclerNewArray( mRecycler, RegSlot, size );
    }

    void AsmJsModuleInfo::EnsureHeapAttached(ScriptFunction * func)
    {
        FrameDisplay* frame = func->GetEnvironment();
        ArrayBuffer* moduleArrayBuffer = *(ArrayBuffer**)((Var*)frame->GetItem(0) + AsmJsModuleMemory::MemoryTableBeginOffset);
        if (moduleArrayBuffer && moduleArrayBuffer->IsDetached())
        {
            Throw::OutOfMemory();
        }
    }

    void * AsmJsModuleInfo::ConvertFrameForJavascript(void * asmMemory, ScriptFunction* func)
    {
        FunctionBody * body = func->GetFunctionBody();
        AsmJsFunctionInfo * asmFuncInfo = body->GetAsmJsFunctionInfo();
        FunctionBody * moduleBody = asmFuncInfo->GetModuleFunctionBody();
        AsmJsModuleInfo * asmModuleInfo = moduleBody->GetAsmJsModuleInfo();
        Assert(asmModuleInfo);

        ScriptContext * scriptContext = func->GetScriptContext();
        // AsmJsModuleEnvironment is all laid out here
        Var * asmJsEnvironment = static_cast<Var*>(func->GetEnvironment()->GetItem(0));
        Var * asmBufferPtr = asmJsEnvironment + asmModuleInfo->GetModuleMemory().mArrayBufferOffset;
        ArrayBuffer * asmBuffer = *asmBufferPtr ? ArrayBuffer::FromVar(*asmBufferPtr) : nullptr;

        Var stdLibObj = *(asmJsEnvironment + asmModuleInfo->GetModuleMemory().mStdLibOffset);
        Var asmMathObject = stdLibObj ? JavascriptOperators::OP_GetProperty(stdLibObj, PropertyIds::Math, scriptContext) : nullptr;

        Var * asmFFIs = asmJsEnvironment + asmModuleInfo->GetModuleMemory().mFFIOffset;
        Var * asmFuncs = asmJsEnvironment + asmModuleInfo->GetModuleMemory().mFuncOffset;
        Var ** asmFuncPtrs = reinterpret_cast<Var**>(asmJsEnvironment + asmModuleInfo->GetModuleMemory().mFuncPtrOffset);

        double * asmDoubleVars = reinterpret_cast<double*>(asmJsEnvironment + asmModuleInfo->GetModuleMemory().mDoubleOffset);
        int * asmIntVars = reinterpret_cast<int*>(asmJsEnvironment + asmModuleInfo->GetModuleMemory().mIntOffset);
        float * asmFloatVars = reinterpret_cast<float*>(asmJsEnvironment + asmModuleInfo->GetModuleMemory().mFloatOffset);

        AsmJsSIMDValue * asmSIMDVars = reinterpret_cast<AsmJsSIMDValue*>(asmJsEnvironment + asmModuleInfo->GetModuleMemory().mSimdOffset);


#if DEBUG
        Var * slotArray = RecyclerNewArrayZ(scriptContext->GetRecycler(), Var, moduleBody->scopeSlotArraySize + ScopeSlots::FirstSlotIndex);
#else
        Var * slotArray = RecyclerNewArray(scriptContext->GetRecycler(), Var, moduleBody->scopeSlotArraySize + ScopeSlots::FirstSlotIndex);
#endif
        ScopeSlots scopeSlots(slotArray);
        scopeSlots.SetCount(moduleBody->scopeSlotArraySize);
        scopeSlots.SetScopeMetadata(moduleBody);

        auto asmSlotMap = asmModuleInfo->GetAsmJsSlotMap();
        Assert((uint)asmModuleInfo->GetSlotsCount() == moduleBody->scopeSlotArraySize);

        Js::ActivationObject* activeScopeObject = nullptr;
        if (moduleBody->GetObjectRegister() != 0)
        {
            activeScopeObject = static_cast<ActivationObject*>(scriptContext->GetLibrary()->CreateActivationObject());
        }

        PropertyId* propertyIdArray = moduleBody->GetPropertyIdsForScopeSlotArray();
        for (int i = 0; i < asmModuleInfo->GetSlotsCount(); ++i)
        {
            AsmJsSlot * asmSlot;
            bool found = asmSlotMap->TryGetValue(propertyIdArray[i], &asmSlot);
            // we should have everything we need in the map
            Assert(found);
            Var value = nullptr;
            switch (asmSlot->symType)
            {
            case AsmJsSymbol::ConstantImport:
            case AsmJsSymbol::Variable:
            {
                switch (asmSlot->varType)
                {
                case AsmJsVarType::Double:
                    value = JavascriptNumber::New(asmDoubleVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Float:
                    value = JavascriptNumber::New(asmFloatVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Int:
                    value = JavascriptNumber::ToVar(asmIntVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Float32x4:
                    value = JavascriptSIMDFloat32x4::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Float64x2:
                    value = JavascriptSIMDFloat64x2::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Int32x4:
                    value = JavascriptSIMDInt32x4::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                default:
                    Assume(UNREACHED);
                }
                break;
            }
            case AsmJsSymbol::ModuleArgument:
            {
                switch (asmSlot->argType)
                {
                case AsmJsModuleArg::ArgType::StdLib:
                    value = stdLibObj;
                    break;
                case AsmJsModuleArg::ArgType::Import:
                    // we can't reference this inside functions (and don't hold onto it), but must set to something, so set it to be undefined
                    value = scriptContext->GetLibrary()->GetUndefined();
                    break;
                case AsmJsModuleArg::ArgType::Heap:
                    value = asmBuffer;
                    break;
                default:
                    Assume(UNREACHED);
                }
                break;
            }
            case AsmJsSymbol::ImportFunction:
                value = asmFFIs[asmSlot->location];
                break;
            case AsmJsSymbol::FuncPtrTable:
                value = JavascriptArray::OP_NewScArrayWithElements(asmSlot->funcTableSize, asmFuncPtrs[asmSlot->location], scriptContext);
                break;
            case AsmJsSymbol::ModuleFunction:
                value = asmFuncs[asmSlot->location];
                break;
            case AsmJsSymbol::MathConstant:
                value = JavascriptNumber::New(asmSlot->mathConstVal, scriptContext);
                break;
            case AsmJsSymbol::ArrayView:
            {
                AnalysisAssert(asmBuffer);
#ifdef _M_X64
                const bool isOptimizedBuffer = true;
#elif _M_IX86
                const bool isOptimizedBuffer = false;
#else
                Assert(UNREACHED);
                const bool isOptimizedBuffer = false;
#endif
                Assert(isOptimizedBuffer == asmBuffer->IsValidVirtualBufferLength(asmBuffer->GetByteLength()));
                switch (asmSlot->viewType)
                {
                case ArrayBufferView::TYPE_FLOAT32:
                    value = TypedArray<float, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength() >> 2, scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_FLOAT64:
                    value = TypedArray<double, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength() >> 3, scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_INT8:
                    value = TypedArray<int8, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength(), scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_INT16:
                    value = TypedArray<int16, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength() >> 1, scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_INT32:
                    value = TypedArray<int32, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength() >> 2, scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_UINT8:
                    value = TypedArray<uint8, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength(), scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_UINT16:
                    value = TypedArray<uint16, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength() >> 1, scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_UINT32:
                    value = TypedArray<uint32, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength() >> 2, scriptContext->GetLibrary());
                    break;
                default:
                    Assume(UNREACHED);
                }
                break;
            }
            case AsmJsSymbol::MathBuiltinFunction:
            {
                switch (asmSlot->builtinMathFunc)
                {
#define ASMJS_MATH_FUNC_NAMES(name, propertyName) \
                        case AsmJSMathBuiltin_##name: \
                            value = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::##propertyName, scriptContext); \
                            break;
#include "AsmJsBuiltinNames.h"
                default:
                    Assume(UNREACHED);
                }
                break;
            }
            case AsmJsSymbol::TypedArrayBuiltinFunction:
                switch (asmSlot->builtinArrayFunc)
                {
#define ASMJS_ARRAY_NAMES(name, propertyName) \
                        case AsmJSTypedArrayBuiltin_##name: \
                            value = JavascriptOperators::OP_GetProperty(stdLibObj, PropertyIds::##propertyName, scriptContext); \
                            break;
#include "AsmJsBuiltinNames.h"
                default:
                    Assume(UNREACHED);
                }
                break;

            case AsmJsSymbol::SIMDBuiltinFunction:
                switch (asmSlot->builtinSIMDFunc)
                {
#define ASMJS_SIMD_NAMES(name, propertyName) \
                        case AsmJsSIMDBuiltin_##name: \
                            value = JavascriptOperators::OP_GetProperty(stdLibObj, PropertyIds::##propertyName, scriptContext); \
                            break;
#include "AsmJsBuiltinNames.h"
                default:
                    Assume(UNREACHED);
                }
                break;

            default:
                Assume(UNREACHED);
            }
            if (activeScopeObject != nullptr)
            {
                activeScopeObject->SetPropertyWithAttributes(
                    propertyIdArray[i],
                    value,
                    asmSlot->isConstVar ? PropertyConstDefaults : PropertyDynamicTypeDefaults,
                    nullptr);
            }
            else
            {
                // ensure we aren't multiply writing to a slot
                Assert(scopeSlots.Get(i) == nullptr);
                scopeSlots.Set(i, value);
            }
        }

        if (activeScopeObject != nullptr)
        {
            return (void*)activeScopeObject;
        }
        else
        {
            return (void*)slotArray;
        }
    }

    bool AsmJsModuleCompiler::LookupStdLibSIMDNameInMap(PropertyName name, AsmJsSIMDFunction **simdFunc, SIMDNameMap* map) const
    {
        return map->TryGetValue(name->GetPropertyId(), simdFunc);
    }

    bool AsmJsModuleCompiler::AddStandardLibrarySIMDNameInMap(PropertyId id, AsmJsSIMDFunction *simdFunc, SIMDNameMap* map)
    {
        //SimdBuiltin simdBuiltin(simdFunc->GetSimdBuiltInFunction(), simdFunc);
        if (map->ContainsKey(id))
        {
            return nullptr;
        }

        return map->AddNew(id, simdFunc) == -1 ? false : true;
    }

    bool AsmJsModuleCompiler::LookupStdLibSIMDName(PropertyId baseId, PropertyName fieldName, AsmJsSIMDFunction **simdFunc)
    {
        switch (baseId)
        {
        case PropertyIds::Int32x4:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDInt32x4Map);
        case PropertyIds::Float32x4:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDFloat32x4Map);
        case PropertyIds::Float64x2:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDFloat64x2Map);
        default:
            AssertMsg(false, "Invalid SIMD type");
            return false;
        }
    }

    bool AsmJsModuleCompiler::LookupStdLibSIMDName(AsmJsSIMDBuiltinFunction baseId, PropertyName fieldName, AsmJsSIMDFunction **simdFunc)
    {
        switch (baseId)
        {
        case AsmJsSIMDBuiltin_Int32x4:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDInt32x4Map);
        case AsmJsSIMDBuiltin_Float32x4:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDFloat32x4Map);
        case AsmJsSIMDBuiltin_Float64x2:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDFloat64x2Map);
        default:
            AssertMsg(false, "Invalid SIMD type");
            return false;
        }
    }

    AsmJsSIMDFunction* AsmJsModuleCompiler::LookupSimdConstructor(PropertyName name)
    {
        AsmJsFunctionDeclaration *func = LookupFunction(name);
        if (func == nullptr || func->GetSymbolType() != AsmJsSymbol::SIMDBuiltinFunction)
        {
            return nullptr;
        }
        AsmJsSIMDFunction *simdFunc = func->Cast<AsmJsSIMDFunction>();
        if (simdFunc->IsConstructor())
        {
            return simdFunc;
        }
        return nullptr;
    }

    AsmJsSIMDFunction* AsmJsModuleCompiler::LookupSimdTypeCheck(PropertyName name)
    {
        AsmJsFunctionDeclaration *func = LookupFunction(name);
        if (func == nullptr || func->GetSymbolType() != AsmJsSymbol::SIMDBuiltinFunction)
        {
            return nullptr;
        }
        AsmJsSIMDFunction *simdFunc = func->Cast<AsmJsSIMDFunction>();
        if (simdFunc->IsTypeCheck())
        {
            return simdFunc;
        }
        return nullptr;
    }

    AsmJsSIMDFunction* AsmJsModuleCompiler::LookupSimdOperation(PropertyName name)
    {
        AsmJsFunctionDeclaration *func = LookupFunction(name);
        if (func == nullptr || func->GetSymbolType() != AsmJsSymbol::SIMDBuiltinFunction)
        {
            return nullptr;
        }
        AsmJsSIMDFunction *simdFunc = func->Cast<AsmJsSIMDFunction>();
        if (simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Int32x4 &&
            simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Float32x4 &&
            simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Float64x2)
        {
            return simdFunc;
        }
        return nullptr;
    }


    bool AsmJsModuleCompiler::AddSimdValueVar(PropertyName name, ParseNode* pnode, AsmJsSIMDFunction* simdFunc)
    {
        AssertMsg(simdFunc->GetSymbolType() == AsmJsSymbol::SIMDBuiltinFunction, "Expecting SIMD builtin");
        AssertMsg(simdFunc->IsConstructor(), "Expecting constructor function");

        AsmJsSIMDValue value;
        AsmJsVarType type = simdFunc->GetConstructorVarType();

        // e.g. var g1 = f4(1.0, 2.0, 3.0, 4.0);
        if (!ValidateSimdConstructor(pnode, simdFunc, value))
        {
            return false;
        }

        AsmJsVar* var = Anew(&mAllocator, AsmJsVar, name);
        if (!var || !DefineIdentifier(name, var))
        {
            return false;
        }

        ++mVarCount;
        var->SetVarType(type);
        var->SetConstInitialiser(value);
        // acquire register
        var->SetLocation(mSimdVarSpace.AcquireRegister());
        return true;
    }

    bool AsmJsModuleCompiler::ValidateSimdConstructor(ParseNode* pnode, AsmJsSIMDFunction* simdFunc, AsmJsSIMDValue& value)
    {
        Assert(pnode->nop == knopCall);

        uint argCount = pnode->sxCall.argCount;
        ParseNode* argNode = pnode->sxCall.pnodeArgs;
        ParseNode *arg = argNode;
        uint nop = 0;
        AsmJsSIMDBuiltinFunction simdBuiltin = simdFunc->GetSimdBuiltInFunction();

        if (!simdFunc->IsConstructor(argCount))
        {
            return Fail(pnode, L"Invalid SIMD constructor or wrong number of arguments.");
        }

        switch (simdBuiltin)
        {
        case AsmJsSIMDBuiltin_Float64x2:
        case AsmJsSIMDBuiltin_Float32x4:
            nop = (uint)knopFlt;
            break;
        case AsmJsSIMDBuiltin_Int32x4:
            nop = (uint)knopInt;
            break;
        default:
            Assert(UNREACHED);
        }

        Assert(simdBuiltin == AsmJsSIMDBuiltin_Float64x2 || simdBuiltin == AsmJsSIMDBuiltin_Float32x4 || simdBuiltin == AsmJsSIMDBuiltin_Int32x4);
        if (simdFunc->GetArgCount() != argCount)
        {
            return Fail(pnode, L"Invalid number of arguments to SIMD constructor.");
        }

        for (uint i = 0; i < argCount; i++)
        {
            arg = argNode;
            if (argNode->nop == knopList)
            {
                arg = ParserWrapper::GetBinaryLeft(argNode);
                argNode = ParserWrapper::GetBinaryRight(argNode);
            }
            Assert(arg);
            // store to SIMD Value
            if (arg->nop == nop)
            {
                if (nop == (uint)knopInt)
                {
                    value.i32[i] = arg->sxInt.lw;
                }
                else if (nop == (uint)knopFlt)
                {
                    if (simdBuiltin == AsmJsSIMDBuiltin_Float32x4)
                    {
                        value.f32[i] = (float)arg->sxFlt.dbl;
                    }
                    else // float64x2
                    {
                        value.f64[i] = arg->sxFlt.dbl;
                    }
                }
            }
            else
            {
                return Fail(pnode, L"Invalid argument type to SIMD constructor.");
            }
        }
        return true;
    }
};
#endif

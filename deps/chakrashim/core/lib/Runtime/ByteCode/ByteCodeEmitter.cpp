//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"
#include "FormalsUtil.h"
#include "Language\AsmJs.h"

void EmitReference(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);
void EmitAssignment(ParseNode *asgnNode, ParseNode *lhs, Js::RegSlot rhsLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);
void EmitLoad(ParseNode *rhs, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);
void EmitCall(ParseNode* pnode, Js::RegSlot rhsLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo, BOOL fReturnValue, BOOL fEvaluateComponents, BOOL fHasNewTarget, Js::RegSlot overrideThisLocation = Js::Constants::NoRegister);
void EmitSuperFieldPatch(FuncInfo* funcInfo, ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator);

bool EmitUseBeforeDeclaration(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);
void EmitUseBeforeDeclarationRuntimeError(ByteCodeGenerator *byteCodeGenerator, Js::RegSlot location, bool fLoadUndef = true);
void VisitClearTmpRegs(ParseNode * pnode, ByteCodeGenerator * byteCodeGenerator, FuncInfo * funcInfo);

bool CallTargetIsArray(ParseNode *pnode)
{
    return pnode->nop == knopName && pnode->sxPid.PropertyIdFromNameNode() == Js::PropertyIds::Array;
}

BOOL MayHaveSideEffectOnNode(ParseNode *pnode, ParseNode *pnodeSE)
{
    // Try to determine whether pnodeSE may kill the named var represented by pnode.

    if (pnode->nop == knopComputedName)
    {
        pnode = pnode->sxUni.pnode1;
    }

    if (pnode->nop != knopName)
    {
        // Only investigating named vars here.
        return false;
    }

    uint fnop = ParseNode::Grfnop(pnodeSE->nop);
    if (fnop & fnopLeaf)
    {
        // pnodeSE is a leaf and can't kill anything.
        return false;
    }

    if (fnop & fnopAsg)
    {
        // pnodeSE is an assignment (=, ++, +=, etc.)
        // Trying to examine the LHS of pnodeSE caused small perf regressions,
        // maybe because of code layout or some other subtle effect.
        return true;
    }

    if (fnop & fnopUni)
    {
        // pnodeSE is a unary op, so recurse to the source (if present - e.g., [] may have no opnd).
        if (pnodeSE->nop == knopTempRef)
        {
            return false;
        }
        else
        {
            return pnodeSE->sxUni.pnode1 && MayHaveSideEffectOnNode(pnode, pnodeSE->sxUni.pnode1);
        }
    }
    else if (fnop & fnopBin)
    {
        // pnodeSE is a binary (or ternary) op, so recurse to the sources (if present).
        if (pnodeSE->nop == knopQmark)
        {
            return MayHaveSideEffectOnNode(pnode, pnodeSE->sxTri.pnode1) ||
                MayHaveSideEffectOnNode(pnode, pnodeSE->sxTri.pnode2) ||
                MayHaveSideEffectOnNode(pnode, pnodeSE->sxTri.pnode3);
        }
        else if (pnodeSE->nop == knopCall || pnodeSE->nop == knopNew)
        {
            return MayHaveSideEffectOnNode(pnode, pnodeSE->sxCall.pnodeTarget) ||
                (pnodeSE->sxCall.pnodeArgs && MayHaveSideEffectOnNode(pnode, pnodeSE->sxCall.pnodeArgs));
        }
        else
        {
            return MayHaveSideEffectOnNode(pnode, pnodeSE->sxBin.pnode1) ||
                (pnodeSE->sxBin.pnode2 && MayHaveSideEffectOnNode(pnode, pnodeSE->sxBin.pnode2));
        }
    }
    else if (pnodeSE->nop == knopList)
    {
        return true;
    }

    return false;
}

bool IsCallOfConstants(ParseNode *pnode);
bool BlockHasOwnScope(ParseNode* pnodeBlock, ByteCodeGenerator *byteCodeGenerator);
bool CreateNativeArrays(ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);

bool IsArguments(ParseNode *pnode)
{
    for (;;)
    {
        switch (pnode->nop)
        {
        case knopName:
            return pnode->sxPid.sym && pnode->sxPid.sym->GetIsArguments();

        case knopCall:
        case knopNew:
            if (IsArguments(pnode->sxCall.pnodeTarget))
            {
                return true;
            }

            if (pnode->sxCall.pnodeArgs)
            {
                ParseNode *pnodeArg = pnode->sxCall.pnodeArgs;
                while (pnodeArg->nop == knopList)
                {
                    if (IsArguments(pnodeArg->sxBin.pnode1))
                        return true;

                    pnodeArg = pnodeArg->sxBin.pnode2;
                }

                pnode = pnodeArg;
                break;
            }

            return false;

        case knopArray:
            if (pnode->sxArrLit.arrayOfNumbers || pnode->sxArrLit.count == 0)
            {
                return false;
            }

            pnode = pnode->sxUni.pnode1;
            break;

        case knopQmark:
            if (IsArguments(pnode->sxTri.pnode1) || IsArguments(pnode->sxTri.pnode2))
            {
                return true;
            }

            pnode = pnode->sxTri.pnode3;
            break;

        //
        // Cases where we don't check for "arguments" yet.
        // Assume that they might have it. Disable the optimization is such scenarios
        //
        case knopList:
        case knopObject:
        case knopVarDecl:
        case knopConstDecl:
        case knopLetDecl:
        case knopFncDecl:
        case knopClassDecl:
        case knopFor:
        case knopIf:
        case knopDoWhile:
        case knopWhile:
        case knopForIn:
        case knopForOf:
        case knopReturn:
        case knopBlock:
        case knopBreak:
        case knopContinue:
        case knopLabel:
        case knopTypeof:
        case knopThrow:
        case knopWith:
        case knopFinally:
        case knopTry:
        case knopTryCatch:
        case knopTryFinally:
            return true;

        default:
        {
            uint flags = ParseNode::Grfnop(pnode->nop);
            if (flags&fnopUni)
            {
                Assert(pnode->sxUni.pnode1);

                pnode = pnode->sxUni.pnode1;
                break;
            }
            else if (flags&fnopBin)
            {
                Assert(pnode->sxBin.pnode1 && pnode->sxBin.pnode2);

                if (IsArguments(pnode->sxBin.pnode1))
                {
                    return true;
                }

                pnode = pnode->sxBin.pnode2;
                break;
            }

            return false;
        }

        }
    }
}

bool ApplyEnclosesArgs(ParseNode* fncDecl, ByteCodeGenerator* byteCodeGenerator);
void Emit(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, BOOL fReturnValue, bool isConstructorCall = false, ParseNode *bindPnode = nullptr);
void EmitComputedFunctionNameVar(ParseNode *nameNode, ParseNode *exprNode, ByteCodeGenerator *byteCodeGenerator);
void EmitBinaryOpnds(ParseNode *pnode1, ParseNode *pnode2, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);
bool IsExpressionStatement(ParseNode* stmt, const Js::ScriptContext *const scriptContext);
void EmitInvoke(Js::RegSlot location, Js::RegSlot callObjLocation, Js::PropertyId propertyId, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo);
void EmitInvoke(Js::RegSlot location, Js::RegSlot callObjLocation, Js::PropertyId propertyId, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo, Js::RegSlot arg1Location);

static const Js::OpCode nopToOp[knopLim] =
{
#define OP(x) Br##x##_A
#define PTNODE(nop,sn,pc,nk,grfnop,json) Js::OpCode::pc,
#include "ptlist.h"
};
static const Js::OpCode nopToCMOp[knopLim] =
{
#define OP(x) Cm##x##_A
#define PTNODE(nop,sn,pc,nk,grfnop,json) Js::OpCode::pc,
#include "ptlist.h"
};

Js::OpCode ByteCodeGenerator::ToChkUndeclOp(Js::OpCode op) const
{
    switch(op)
    {
    case Js::OpCode::StLocalSlot:
        return Js::OpCode::StLocalSlotChkUndecl;

    case Js::OpCode::StInnerSlot:
        return Js::OpCode::StInnerSlotChkUndecl;

    case Js::OpCode::StEnvSlot:
        return Js::OpCode::StEnvSlotChkUndecl;

    case Js::OpCode::StObjSlot:
        return Js::OpCode::StObjSlotChkUndecl;

    case Js::OpCode::StLocalObjSlot:
        return Js::OpCode::StLocalObjSlotChkUndecl;

    case Js::OpCode::StEnvObjSlot:
        return Js::OpCode::StEnvObjSlotChkUndecl;

    default:
        AssertMsg(false, "Unknown opcode for chk undecl mapping");
        return Js::OpCode::InvalidOpCode;
    }
}

// Tracks a register slot let/const property for the passed in debugger block/catch scope.
// debuggerScope         - The scope to add the variable to.
// symbol                - The symbol that represents the register property.
// funcInfo              - The function info used to store the property into the tracked debugger register slot list.
// flags                 - The flags to assign to the property.
// isFunctionDeclaration - Whether or not the register is a function declaration, which requires that its byte code offset be updated immediately.
void ByteCodeGenerator::TrackRegisterPropertyForDebugger(
    Js::DebuggerScope *debuggerScope,
    Symbol *symbol,
    FuncInfo *funcInfo,
    Js::DebuggerScopePropertyFlags flags /*= Js::DebuggerScopePropertyFlags_None*/,
    bool isFunctionDeclaration /*= false*/)
{
    Assert(debuggerScope);
    Assert(symbol);
    Assert(funcInfo);

    Js::RegSlot location = symbol->GetLocation();

    Js::DebuggerScope *correctDebuggerScope = debuggerScope;
    if (debuggerScope->scopeType != Js::DiagExtraScopesType::DiagBlockScopeDirect && debuggerScope->scopeType != Js::DiagExtraScopesType::DiagCatchScopeDirect)
    {
        // We have to get the appropriate scope and add property over there.
        // Make sure the scope is created whether we're in debug mode or not, because we
        // need the empty scopes present during reparsing for debug mode.
        correctDebuggerScope = debuggerScope->GetSiblingScope(location, Writer()->GetFunctionWrite());
    }

    if (this->ShouldTrackDebuggerMetadata() && !symbol->GetIsTrackedForDebugger())
    {
        // Only track the property if we're in debug mode since it's only needed by the debugger.
        Js::PropertyId propertyId = symbol->EnsurePosition(this);

        this->Writer()->AddPropertyToDebuggerScope(
            correctDebuggerScope,
            location,
            propertyId,
            /*shouldConsumeRegister*/ true,
            flags,
            isFunctionDeclaration);

        Js::FunctionBody *byteCodeFunction = funcInfo->GetParsedFunctionBody();
        byteCodeFunction->InsertSymbolToRegSlotList(location, propertyId, funcInfo->varRegsCount);

        symbol->SetIsTrackedForDebugger(true);
    }
}

void ByteCodeGenerator::TrackActivationObjectPropertyForDebugger(
    Js::DebuggerScope *debuggerScope,
    Symbol *symbol,
    Js::DebuggerScopePropertyFlags flags /*= Js::DebuggerScopePropertyFlags_None*/,
    bool isFunctionDeclaration /*= false*/)
{
    Assert(debuggerScope);
    Assert(symbol);

    // Only need to track activation object properties in debug mode.
    if (ShouldTrackDebuggerMetadata() && !symbol->GetIsTrackedForDebugger())
    {
        Js::RegSlot location = symbol->GetLocation();
        Js::PropertyId propertyId = symbol->EnsurePosition(this);

        this->Writer()->AddPropertyToDebuggerScope(
            debuggerScope,
            location,
            propertyId,
            /*shouldConsumeRegister*/ false,
            flags,
            isFunctionDeclaration);

        symbol->SetIsTrackedForDebugger(true);
    }
}

void ByteCodeGenerator::TrackSlotArrayPropertyForDebugger(
    Js::DebuggerScope *debuggerScope,
    Symbol* symbol,
    Js::PropertyId propertyId,
    Js::DebuggerScopePropertyFlags flags /*= Js::DebuggerScopePropertyFlags_None*/,
    bool isFunctionDeclaration /*= false*/)
{
    // Note: Slot array properties are tracked even in non-debug mode in order to support slot array serialization
    // of let/const variables between non-debug and debug mode (for example, when a slot array var escapes and is retrieved
    // after a debugger attach or for WWA apps).  They are also needed for heap enumeration.
    Assert(debuggerScope);
    Assert(symbol);

    if (!symbol->GetIsTrackedForDebugger())
    {
        Js::RegSlot location = symbol->GetScopeSlot();
        Assert(location != Js::Constants::NoRegister);
        Assert(propertyId != Js::Constants::NoProperty);

        this->Writer()->AddPropertyToDebuggerScope(
            debuggerScope,
            location,
            propertyId,
            /*shouldConsumeRegister*/ false,
            flags,
            isFunctionDeclaration);

        symbol->SetIsTrackedForDebugger(true);
    }
}

// Tracks a function declaration inside a block scope for the debugger metadata's current scope (let binding).
void ByteCodeGenerator::TrackFunctionDeclarationPropertyForDebugger(Symbol *functionDeclarationSymbol, FuncInfo *funcInfoParent)
{
    Assert(functionDeclarationSymbol);
    Assert(funcInfoParent);
    AssertMsg(functionDeclarationSymbol->GetIsBlockVar(), "We should only track inner function let bindings for the debugger.");

    // Note: we don't have to check symbol->GetIsTrackedForDebugger, as we are not doing actual work here,
    //       which is done in other Track* functions that we call.

    if (functionDeclarationSymbol->IsInSlot(funcInfoParent))
    {
        if (functionDeclarationSymbol->GetScope()->GetIsObject())
        {
            this->TrackActivationObjectPropertyForDebugger(
                this->Writer()->GetCurrentDebuggerScope(),
                functionDeclarationSymbol,
                Js::DebuggerScopePropertyFlags_None,
                true /*isFunctionDeclaration*/);
        }
        else
        {
            // Make sure the property has a slot. This will bump up the size of the slot array if necessary.
            // Note that slot array inner function bindings are tracked even in non-debug mode in order
            // to keep the lifetime of the closure binding that could escape around for heap enumeration.
            functionDeclarationSymbol->EnsureScopeSlot(funcInfoParent);
            functionDeclarationSymbol->EnsurePosition(this);
            this->TrackSlotArrayPropertyForDebugger(
                this->Writer()->GetCurrentDebuggerScope(),
                functionDeclarationSymbol,
                functionDeclarationSymbol->GetPosition(),
                Js::DebuggerScopePropertyFlags_None,
                true /*isFunctionDeclaration*/);
        }
    }
    else
    {
        this->TrackRegisterPropertyForDebugger(
            this->Writer()->GetCurrentDebuggerScope(),
            functionDeclarationSymbol,
            funcInfoParent,
            Js::DebuggerScopePropertyFlags_None,
            true /*isFunctionDeclaration*/);
    }
}

// Updates the byte code offset of the property with the passed in location and ID.
// Used to track let/const variables that are in the dead zone debugger side.
// location                 - The activation object, scope slot index, or register location for the property.
// propertyId               - The ID of the property to update.
// shouldConsumeRegister    - Whether or not the a register should be consumed (used for reg slot locations).
void ByteCodeGenerator::UpdateDebuggerPropertyInitializationOffset(Js::RegSlot location, Js::PropertyId propertyId, bool shouldConsumeRegister)
{
    Assert(this->Writer());
    Js::DebuggerScope* currentDebuggerScope = this->Writer()->GetCurrentDebuggerScope();
    Assert(currentDebuggerScope);
    if (currentDebuggerScope != nullptr)
    {
        this->Writer()->UpdateDebuggerPropertyInitializationOffset(
            currentDebuggerScope,
            location,
            propertyId,
            shouldConsumeRegister);
    }
}

void ByteCodeGenerator::LoadHeapArguments(FuncInfo *funcInfo)
{
    if (funcInfo->GetHasCachedScope())
    {
        this->LoadCachedHeapArguments(funcInfo);
    }
    else
    {
        this->LoadUncachedHeapArguments(funcInfo);
    }
}

void GetFormalArgsArray(ByteCodeGenerator *byteCodeGenerator, FuncInfo * funcInfo, Js::PropertyIdArray *propIds)
{
    Assert(funcInfo);
    Assert(propIds);
    Assert(byteCodeGenerator);

    bool hadDuplicates = false;
    Js::ArgSlot i = 0;

    auto processArg = [&](ParseNode *pnode)
    {
        if (pnode->IsVarLetOrConst())
        {
            Assert(i < propIds->count);
            Symbol *sym = pnode->sxVar.sym;
            Assert(sym);
            Js::PropertyId symPos = sym->EnsurePosition(byteCodeGenerator);

            //
            // Check if the function has any same name parameters
            // For the same name param, only the last one will be passed the correct propertyid
            // For remaining dup param names, pass Constants::NoProperty
            //
            for (Js::ArgSlot j = 0; j < i; j++)
            {
                if (propIds->elements[j] == symPos)
                {
                    // Found a dup parameter name
                    propIds->elements[j] = Js::Constants::NoProperty;
                    hadDuplicates = true;
                    break;
                }
            }
            propIds->elements[i] = symPos;
        }
        else
        {
            propIds->elements[i] = Js::Constants::NoProperty;
        }
        ++i;
    };
    MapFormals(funcInfo->root, processArg);

    propIds->hadDuplicates = hadDuplicates;
}

void ByteCodeGenerator::LoadUncachedHeapArguments(FuncInfo *funcInfo)
{
    Assert(funcInfo->GetHasHeapArguments());

    Scope *scope = funcInfo->GetBodyScope();
    Assert(scope);
    Symbol *argSym = funcInfo->GetArgumentsSymbol();
    Assert(argSym && argSym->GetIsArguments());
    Js::RegSlot argumentsLoc = argSym->GetLocation();
    Js::RegSlot propIdsLoc = funcInfo->nullConstantRegister;

    Js::OpCode opcode = funcInfo->root->sxFnc.IsSimpleParameterList() ? Js::OpCode::LdHeapArguments : Js::OpCode::LdLetHeapArguments;
    bool hasRest = funcInfo->root->sxFnc.pnodeRest != nullptr;
    uint count = funcInfo->inArgsCount + (hasRest ? 1 : 0) - 1;
    if (count == 0)
    {
        // If no formals to function (only "this"), then no need to create the scope object.
        // Leave both the arguments location and the propertyIds location as null.
        Assert(funcInfo->root->sxFnc.pnodeArgs == nullptr && !hasRest);
    }
    else if (!NeedScopeObjectForArguments(funcInfo, funcInfo->root))
    {
        // We may not need a scope object for arguments, e.g. strict mode with no eval.
    }
    else if (funcInfo->frameObjRegister != Js::Constants::NoRegister)
    {
        // Pass the frame object and ID array to the runtime, and put the resulting Arguments object
        // at the expected location.
        propIdsLoc = argumentsLoc;

        Js::PropertyIdArray *propIds = AnewPlus(GetAllocator(), count * sizeof(Js::PropertyId), Js::PropertyIdArray, count);

        GetFormalArgsArray(this, funcInfo, propIds);

        // Generate the opcode with propIds
        Writer()->Auxiliary(
            Js::OpCode::LdPropIds,
            propIdsLoc,
            propIds,
            sizeof(Js::PropertyIdArray) + count * sizeof(Js::PropertyId),
            count);

        AdeletePlus(GetAllocator(), count * sizeof(Js::PropertyId), propIds);
    }

    this->m_writer.Reg2(opcode, argumentsLoc, propIdsLoc);
    EmitLocalPropInit(argSym->GetLocation(), argSym, funcInfo);
}

void ByteCodeGenerator::LoadCachedHeapArguments(FuncInfo *funcInfo)
{
    Assert(funcInfo->GetHasHeapArguments());

    Scope *scope = funcInfo->GetBodyScope();
    Assert(scope);
    Symbol *argSym = funcInfo->GetArgumentsSymbol();
    Assert(argSym && argSym->GetIsArguments());
    Js::RegSlot argumentsLoc = argSym->GetLocation();

    Js::OpCode op = funcInfo->root->sxFnc.IsSimpleParameterList() ? Js::OpCode::LdHeapArgsCached : Js::OpCode::LdLetHeapArgsCached;

    this->m_writer.Reg1(op, argumentsLoc);
    EmitLocalPropInit(argumentsLoc, argSym, funcInfo);
}

Js::JavascriptArray* ByteCodeGenerator::BuildArrayFromStringList(ParseNode* stringNodeList, uint arrayLength, Js::ScriptContext* scriptContext)
{
    Assert(stringNodeList);

    uint index = 0;
    Js::Var str;
    IdentPtr pid;
    Js::JavascriptArray* pArr = scriptContext->GetLibrary()->CreateArray(arrayLength);

    while (stringNodeList->nop == knopList)
    {
        Assert(stringNodeList->sxBin.pnode1->nop == knopStr);

        pid = stringNodeList->sxBin.pnode1->sxPid.pid;
        str = Js::JavascriptString::NewCopyBuffer(pid->Psz(), pid->Cch(), scriptContext);
        pArr->SetItemWithAttributes(index, str, PropertyEnumerable);

        stringNodeList = stringNodeList->sxBin.pnode2;
        index++;
    }

    Assert(stringNodeList->nop == knopStr);

    pid = stringNodeList->sxPid.pid;
    str = Js::JavascriptString::NewCopyBuffer(pid->Psz(), pid->Cch(), scriptContext);
    pArr->SetItemWithAttributes(index, str, PropertyEnumerable);

    return pArr;
}

// For now, this just assigns field ids for the current script.
// Later, we will combine this information with the global field id map.
// This temporary code will not work if a global member is accessed both with and without a LHS.
void ByteCodeGenerator::AssignPropertyIds(Js::ParseableFunctionInfo* functionInfo)
{
    globalScope->ForEachSymbol([this, functionInfo](Symbol * sym)
    {
        this->AssignPropertyId(sym, functionInfo);
    });
}

void ByteCodeGenerator::InitBlockScopedContent(ParseNode *pnodeBlock, Js::DebuggerScope* debuggerScope, FuncInfo *funcInfo)
{
    Assert(pnodeBlock->nop == knopBlock);

    auto genBlockInit = [this, debuggerScope, funcInfo](ParseNode *pnode)
    {
        // Only check if the scope is valid when let/const vars are in the scope.  If there are no let/const vars,
        // the debugger scope will not be created.
        AssertMsg(debuggerScope, "Missing a case of scope tracking in BeginEmitBlock.");

        FuncInfo *funcInfo = this->TopFuncInfo();
        Symbol *sym = pnode->sxVar.sym;
        Scope *scope = sym->GetScope();

        if (sym->GetIsGlobal())
        {
            Js::PropertyId propertyId = sym->EnsurePosition(this);
            if (this->flags & fscrEval)
            {
                AssertMsg(this->IsConsoleScopeEval(), "Let/Consts cannot be in global scope outside of console eval");
                Js::OpCode op = (sym->GetDecl()->nop == knopConstDecl) ? Js::OpCode::InitUndeclConsoleConstFld : Js::OpCode::InitUndeclConsoleLetFld;
                this->m_writer.ElementScopedU(op, funcInfo->FindOrAddReferencedPropertyId(propertyId));
            }
            else
            {
                Js::OpCode op = (sym->GetDecl()->nop == knopConstDecl) ?
                    Js::OpCode::InitUndeclRootConstFld : Js::OpCode::InitUndeclRootLetFld;
                this->m_writer.ElementRootU(op, funcInfo->FindOrAddReferencedPropertyId(propertyId));
            }
        }
        else if (sym->IsInSlot(funcInfo))
        {
            if (scope->GetIsObject())
            {
                Js::RegSlot scopeLocation = scope->GetLocation();
                Js::PropertyId propertyId = sym->EnsurePosition(this);

                if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
                {
                    uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, true);

                    Js::OpCode op = (sym->GetDecl()->nop == knopConstDecl) ?
                        Js::OpCode::InitUndeclLocalConstFld : Js::OpCode::InitUndeclLocalLetFld;

                    this->m_writer.ElementP(op, ByteCodeGenerator::ReturnRegister, cacheId);
                }
                else
                {
                    uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->InnerScopeToRegSlot(scope), propertyId, false, true);

                    Js::OpCode op = (sym->GetDecl()->nop == knopConstDecl) ?
                        Js::OpCode::InitUndeclConstFld : Js::OpCode::InitUndeclLetFld;

                    this->m_writer.ElementPIndexed(op, ByteCodeGenerator::ReturnRegister, scope->GetInnerScopeIndex(), cacheId);
                }

                TrackActivationObjectPropertyForDebugger(debuggerScope, sym, pnode->nop == knopConstDecl ? Js::DebuggerScopePropertyFlags_Const : Js::DebuggerScopePropertyFlags_None);
            }
            else if (CONFIG_FLAG(TDZ))
            {
                Js::RegSlot tmpReg = funcInfo->AcquireTmpRegister();
                this->m_writer.Reg1(Js::OpCode::InitUndecl, tmpReg);
                this->EmitLocalPropInit(tmpReg, sym, funcInfo);
                funcInfo->ReleaseTmpRegister(tmpReg);

                // Slot array properties are tracked in non-debug mode as well because they need to stay
                // around for heap enumeration and escaping during attach/detach.
                TrackSlotArrayPropertyForDebugger(debuggerScope, sym, sym->EnsurePosition(this), pnode->nop == knopConstDecl ? Js::DebuggerScopePropertyFlags_Const : Js::DebuggerScopePropertyFlags_None);
            }
        }
        else
        {
            if (sym->GetDecl()->sxVar.isSwitchStmtDecl && CONFIG_FLAG(TDZ))
            {
                // let/const declared in a switch is the only case of a variable that must be checked for
                // use-before-declaration dynamically within its own function.
                this->m_writer.Reg1(Js::OpCode::InitUndecl, sym->GetLocation());
            }
            // Syms that begin in register may be delay-captured. In debugger mode, such syms
            // will live only in slots, so tell the debugger to find them there.
            if (sym->NeedsSlotAlloc(funcInfo))
            {
                TrackSlotArrayPropertyForDebugger(debuggerScope, sym, sym->EnsurePosition(this), pnode->nop == knopConstDecl ? Js::DebuggerScopePropertyFlags_Const : Js::DebuggerScopePropertyFlags_None);
            }
            else
            {
                TrackRegisterPropertyForDebugger(debuggerScope, sym, funcInfo, pnode->nop == knopConstDecl ? Js::DebuggerScopePropertyFlags_Const : Js::DebuggerScopePropertyFlags_None);
            }
        }
    };

    IterateBlockScopedVariables(pnodeBlock, genBlockInit);
}

// Records the start of a debugger scope if the passed in node has any let/const variables (or is not a block node).
// If it has no let/const variables, nullptr will be returned as no scope will be created.
Js::DebuggerScope* ByteCodeGenerator::RecordStartScopeObject(ParseNode *pnodeBlock, Js::DiagExtraScopesType scopeType, Js::RegSlot scopeLocation /*= Js::Constants::NoRegister*/, int* index /*= nullptr*/)
{
    Assert(pnodeBlock);
    if (pnodeBlock->nop == knopBlock && !pnodeBlock->sxBlock.HasBlockScopedContent())
    {
        // In order to reduce allocations now that we track debugger scopes in non-debug mode,
        // don't add a block to the chain if it has no let/const variables at all.
        return nullptr;
    }

    return this->Writer()->RecordStartScopeObject(scopeType, scopeLocation, index);
}

// Records the end of the current scope, but only if the current block has block scoped content.
// Otherwise, a scope would not have been added (see ByteCodeGenerator::RecordStartScopeObject()).
void ByteCodeGenerator::RecordEndScopeObject(ParseNode *pnodeBlock)
{
    Assert(pnodeBlock);
    if (pnodeBlock->nop == knopBlock && !pnodeBlock->sxBlock.HasBlockScopedContent())
    {
        return;
    }

    this->Writer()->RecordEndScopeObject();
}

void BeginEmitBlock(ParseNode *pnodeBlock, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    Js::DebuggerScope* debuggerScope = nullptr;

    if (BlockHasOwnScope(pnodeBlock, byteCodeGenerator))
    {
        Scope *scope = pnodeBlock->sxBlock.scope;
        byteCodeGenerator->PushScope(scope);

        Js::RegSlot scopeLocation = scope->GetLocation();
        if (scope->GetMustInstantiate())
        {
            Assert(scopeLocation == Js::Constants::NoRegister);
            scopeLocation = funcInfo->FirstInnerScopeReg() + scope->GetInnerScopeIndex();

            if (scope->GetIsObject())
            {
                debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnodeBlock, Js::DiagExtraScopesType::DiagBlockScopeInObject, scopeLocation);

                byteCodeGenerator->Writer()->Unsigned1(Js::OpCode::NewBlockScope, scope->GetInnerScopeIndex());
            }
            else
            {
                int scopeIndex = Js::DebuggerScope::InvalidScopeIndex;
                debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnodeBlock, Js::DiagExtraScopesType::DiagBlockScopeInSlot, scopeLocation, &scopeIndex);

                // TODO: Handle heap enumeration
                int scopeSlotCount = scope->GetScopeSlotCount();
                byteCodeGenerator->Writer()->Num3(Js::OpCode::NewInnerScopeSlots, scope->GetInnerScopeIndex(), scopeSlotCount + Js::ScopeSlots::FirstSlotIndex, scopeIndex);
            }
        }
        else
        {
            // In the direct register access case, there is no block scope emitted but we can still track
            // the start and end offset of the block.  The location registers for let/const variables will still be
            // captured along with this range in InitBlockScopedContent().
            debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnodeBlock, Js::DiagExtraScopesType::DiagBlockScopeDirect);
        }

        bool const isGlobalEvalBlockScope = scope->IsGlobalEvalBlockScope();
        Js::RegSlot frameDisplayLoc = Js::Constants::NoRegister;
        Js::RegSlot tmpInnerEnvReg = Js::Constants::NoRegister;
        ParseNodePtr pnodeScope;
        for (pnodeScope = pnodeBlock->sxBlock.pnodeScopes; pnodeScope;)
        {
            switch (pnodeScope->nop)
            {
            case knopFncDecl:
                if (pnodeScope->sxFnc.IsDeclaration())
                {
                    // The frameDisplayLoc register's lifetime has to be controlled by this function. We can't let
                    // it be released by DefineOneFunction, because further iterations of this loop can allocate
                    // temps, and we can't let frameDisplayLoc be re-purposed until this loop completes.
                    // So we'll supply a temp that we allocate and release here.
                    if (frameDisplayLoc == Js::Constants::NoRegister)
                    {
                        if (funcInfo->frameDisplayRegister != Js::Constants::NoRegister)
                        {
                            frameDisplayLoc = funcInfo->frameDisplayRegister;
                        }
                        else
                        {
                            frameDisplayLoc = funcInfo->GetEnvRegister();
                        }
                        tmpInnerEnvReg = funcInfo->AcquireTmpRegister();
                        frameDisplayLoc = byteCodeGenerator->PrependLocalScopes(frameDisplayLoc, tmpInnerEnvReg, funcInfo);
                    }
                    byteCodeGenerator->DefineOneFunction(pnodeScope, funcInfo, true, frameDisplayLoc);
                }

                // If this is the global eval block scope, the function is actually assigned to the global
                // so we don't need to keep the registers.
                if (isGlobalEvalBlockScope)
                {
                    funcInfo->ReleaseLoc(pnodeScope);
                    pnodeScope->location = Js::Constants::NoRegister;
                }
                pnodeScope = pnodeScope->sxFnc.pnodeNext;
                break;

            case knopBlock:
                pnodeScope = pnodeScope->sxBlock.pnodeNext;
                break;

            case knopCatch:
                pnodeScope = pnodeScope->sxCatch.pnodeNext;
                break;

            case knopWith:
                pnodeScope = pnodeScope->sxWith.pnodeNext;
                break;
            }
        }

        if (tmpInnerEnvReg != Js::Constants::NoRegister)
        {
            funcInfo->ReleaseTmpRegister(tmpInnerEnvReg);
        }

        if (pnodeBlock->sxBlock.scope->IsGlobalEvalBlockScope() && funcInfo->thisScopeSlot != Js::Constants::NoRegister)
        {
            Scope* scope = funcInfo->GetGlobalEvalBlockScope();
            byteCodeGenerator->EmitInitCapturedThis(funcInfo, scope);
        }
    }
    else
    {
        Scope *scope = pnodeBlock->sxBlock.scope;
        if (scope)
        {
            if (scope->GetMustInstantiate())
            {
                debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnodeBlock, Js::DiagExtraScopesType::DiagBlockScopeInObject);
            }
            else
            {
                debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnodeBlock, Js::DiagExtraScopesType::DiagBlockScopeDirect);
            }
        }
        else
        {
            debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnodeBlock, Js::DiagExtraScopesType::DiagBlockScopeInSlot);
        }
    }

    byteCodeGenerator->InitBlockScopedContent(pnodeBlock, debuggerScope, funcInfo);
}

void EndEmitBlock(ParseNode *pnodeBlock, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    if (BlockHasOwnScope(pnodeBlock, byteCodeGenerator))
    {
        Scope *scope = pnodeBlock->sxBlock.scope;
        Assert(scope);
        Assert(scope == byteCodeGenerator->GetCurrentScope());
        byteCodeGenerator->PopScope();
    }

    byteCodeGenerator->RecordEndScopeObject(pnodeBlock);
}

void CloneEmitBlock(ParseNode *pnodeBlock, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    if (BlockHasOwnScope(pnodeBlock, byteCodeGenerator))
    {
        // Only let variables have observable behavior when there are per iteration
        // bindings.  const variables do not since they are immutable.  Therefore,
        // (and the spec agrees), only create new scope clones if the loop variable
        // is a let declaration.
        bool isConst = false;
        pnodeBlock->sxBlock.scope->ForEachSymbolUntil([&isConst](Symbol * const sym) {
            // Exploit the fact that a for loop sxBlock can only have let and const
            // declarations, and can only have one or the other, regardless of how
            // many syms there might be.  Thus only check the first sym.
            isConst = sym->GetDecl()->nop == knopConstDecl;
            return true;
        });

        if (!isConst)
        {
            Scope *scope = pnodeBlock->sxBlock.scope;
            Assert(scope == byteCodeGenerator->GetCurrentScope());

            if (scope->GetMustInstantiate())
            {
                Js::OpCode op = scope->GetIsObject() ? Js::OpCode::CloneBlockScope : Js::OpCode::CloneInnerScopeSlots;

                byteCodeGenerator->Writer()->Unsigned1(op, scope->GetInnerScopeIndex());
            }
        }
    }
}

void EmitBlock(ParseNode *pnodeBlock, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, BOOL fReturnValue)
{
    Assert(pnodeBlock->nop == knopBlock);
    ParseNode *pnode = pnodeBlock->sxBlock.pnodeStmt;
    if (pnode == nullptr)
    {
        return;
    }

    BeginEmitBlock(pnodeBlock, byteCodeGenerator, funcInfo);

    ParseNode *pnodeLastValStmt = pnodeBlock->sxBlock.pnodeLastValStmt;

    while (pnode->nop == knopList)
    {
        ParseNode* stmt = pnode->sxBin.pnode1;
        if (stmt == pnodeLastValStmt)
        {
            // This is the last guaranteed return value, so any potential return values have to be
            // copied to the return register from this point forward.
            pnodeLastValStmt = nullptr;
        }
        byteCodeGenerator->EmitTopLevelStatement(stmt, funcInfo, fReturnValue && (pnodeLastValStmt == nullptr));
        pnode = pnode->sxBin.pnode2;
    }

    if (pnode == pnodeLastValStmt)
    {
        pnodeLastValStmt = nullptr;
    }
    byteCodeGenerator->EmitTopLevelStatement(pnode, funcInfo, fReturnValue && (pnodeLastValStmt == nullptr));

    EndEmitBlock(pnodeBlock, byteCodeGenerator, funcInfo);
}

void ClearTmpRegs(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, FuncInfo* emitFunc)
{
    if (emitFunc->IsTmpReg(pnode->location))
    {
        pnode->location = Js::Constants::NoRegister;
    }
}

void ByteCodeGenerator::EmitTopLevelStatement(ParseNode *stmt, FuncInfo *funcInfo, BOOL fReturnValue)
{
    if (stmt->nop == knopFncDecl && stmt->sxFnc.IsDeclaration())
    {
        // Function declarations (not function-declaration RHS's) are already fully processed.
        // Skip them here so the temp registers don't get messed up.
        return;
    }

    if (stmt->nop == knopName || stmt->nop == knopDot)
    {
        // Generating span for top level names are mostly useful in debugging mode, because user can debug it even though no side-effect expected.
        // But the name can have runtime error, e.g., foo.bar; // where foo is not defined.
        // At this time we need to throw proper line number and offset. so recording on all modes will be useful.
        StartStatement(stmt);
        Writer()->Empty(Js::OpCode::Nop);
        EndStatement(stmt);
    }

    Emit(stmt, this, funcInfo, fReturnValue);
    if (funcInfo->IsTmpReg(stmt->location))
    {
        if (!stmt->isUsed && !fReturnValue)
        {
            m_writer.Reg1(Js::OpCode::Unused, stmt->location);
        }
        funcInfo->ReleaseLoc(stmt);
    }
}

// ByteCodeGenerator::DefineFunctions
//
// Emit byte code for scope-wide function definitions before any calls in the scope, regardless of lexical
// order. Note that stores to the closure array are not emitted until we see the knopFncDecl in the tree
// to make sure that sources of the stores have been defined.
void ByteCodeGenerator::DefineFunctions(FuncInfo *funcInfoParent)
{
    // DefineCachedFunctions doesn't depend on whether the user vars are declared or not, so
    // we'll just overload this variable to mean that the functions getting called again and we don't need to do anything
    if (funcInfoParent->GetHasCachedScope())
    {
        this->DefineCachedFunctions(funcInfoParent);
    }
    else
    {
        this->DefineUncachedFunctions(funcInfoParent);
    }
}

// Iterate over all child functions in a function's parameter and body scopes.
template<typename Fn>
void MapContainerScopeFunctions(ParseNode* pnodeScope, Fn fn)
{
    auto mapFncDeclsInScopeList = [&](ParseNode *pnodeHead)
    {
        for (ParseNode *pnode = pnodeHead; pnode != nullptr;)
        {
            switch (pnode->nop)
            {
            case knopFncDecl:
                fn(pnode);
                pnode = pnode->sxFnc.pnodeNext;
                break;

            case knopBlock:
                pnode = pnode->sxBlock.pnodeNext;
                break;

            case knopCatch:
                pnode = pnode->sxCatch.pnodeNext;
                break;

            case knopWith:
                pnode = pnode->sxWith.pnodeNext;
                break;

            default:
                AssertMsg(false, "Unexpected opcode in tree of scopes");
                return;
            }
        }
    };
    pnodeScope->sxFnc.MapContainerScopes(mapFncDeclsInScopeList);
}

void ByteCodeGenerator::DefineCachedFunctions(FuncInfo *funcInfoParent)
{
    ParseNode *pnodeParent = funcInfoParent->root;
    uint slotCount = 0;

    auto countFncSlots = [&](ParseNode *pnodeFnc)
    {
        if (pnodeFnc->sxFnc.GetFuncSymbol() != nullptr && pnodeFnc->sxFnc.IsDeclaration())
        {
            slotCount++;
        }
    };
    MapContainerScopeFunctions(pnodeParent, countFncSlots);

    if (slotCount == 0)
    {
        return;
    }

    size_t extraBytesActual = AllocSizeMath::Mul(slotCount, sizeof(Js::FuncInfoEntry));
    // Reg2Aux takes int for byteCount so we need to convert to int. OOM if we can't because it would truncate data.
    if (extraBytesActual > INT_MAX)
    {
        Js::Throw::OutOfMemory();
    }
    int extraBytes = (int)extraBytesActual;

    Js::FuncInfoArray *info = AnewPlus(alloc, extraBytes, Js::FuncInfoArray, slotCount);

    slotCount = 0;

    auto fillEntries = [&](ParseNode *pnodeFnc)
    {
        Symbol *sym = pnodeFnc->sxFnc.GetFuncSymbol();
        if (sym != nullptr && (pnodeFnc->sxFnc.IsDeclaration()))
        {
            AssertMsg(!pnodeFnc->sxFnc.IsGenerator(), "Generator functions are not supported by InitCachedFuncs but since they always escape they should disable function caching");
            Js::FuncInfoEntry *entry = &info->elements[slotCount];
            entry->nestedIndex = pnodeFnc->sxFnc.nestedIndex;
            entry->scopeSlot = sym->GetScopeSlot();
            slotCount++;
        }
    };
    MapContainerScopeFunctions(pnodeParent, fillEntries);

    m_writer.AuxNoReg(Js::OpCode::InitCachedFuncs,
        info,
        sizeof(Js::FuncInfoArray) + extraBytes,
        sizeof(Js::FuncInfoArray) + extraBytes);

    slotCount = 0;
    auto defineOrGetCachedFunc = [&](ParseNode *pnodeFnc)
    {
        Symbol *sym = pnodeFnc->sxFnc.GetFuncSymbol();
        if (pnodeFnc->sxFnc.IsDeclaration())
        {
            // Do we need to define the function here (i.e., is it not one of our cached locals)?
            // Only happens if the sym is null (e.g., function x.y(){}).
            if (sym == nullptr)
            {
                this->DefineOneFunction(pnodeFnc, funcInfoParent);
            }
            else if (!sym->IsInSlot(funcInfoParent) && sym->GetLocation() != Js::Constants::NoRegister)
            {
                // If it was defined by InitCachedFuncs, do we need to put it in a register rather than a slot?
                m_writer.Reg1Unsigned1(Js::OpCode::GetCachedFunc, sym->GetLocation(), slotCount);
            }
            // The "x = function() {...}" case is being generated on the fly, during emission,
            // so the caller expects to be able to release this register.
            funcInfoParent->ReleaseLoc(pnodeFnc);
            pnodeFnc->location = Js::Constants::NoRegister;
            slotCount++;
        }
    };
    MapContainerScopeFunctions(pnodeParent, defineOrGetCachedFunc);

    AdeletePlus(alloc, extraBytes, info);
}

void ByteCodeGenerator::DefineUncachedFunctions(FuncInfo *funcInfoParent)
{
    ParseNode *pnodeParent = funcInfoParent->root;
    auto defineCheck = [&](ParseNode *pnodeFnc)
    {
        Assert(pnodeFnc->nop == knopFncDecl);

        //
        // Don't define the function upfront in following cases
        // 1. x = function() {...};
        //    Don't define the function for all modes.
        //    Such a function can only be accessed via the LHS, so we define it at the assignment point
        //    rather than the scope entry to save a register (and possibly save the whole definition).
        //
        // 2. x = function f() {...};
        //    f is not visible in the enclosing scope.
        //    Such function expressions should be emitted only at the assignment point, as can be used only
        //    after the assignment. Might save register.
        //

        if (pnodeFnc->sxFnc.IsDeclaration())
        {
            this->DefineOneFunction(pnodeFnc, funcInfoParent);
            // The "x = function() {...}" case is being generated on the fly, during emission,
            // so the caller expects to be able to release this register.
            funcInfoParent->ReleaseLoc(pnodeFnc);
            pnodeFnc->location = Js::Constants::NoRegister;
        }
    };
    MapContainerScopeFunctions(pnodeParent, defineCheck);
}

void EmitAssignmentToFuncName(ParseNode *pnodeFnc, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfoParent)
{
    // Assign the location holding the func object reference to the given name.
    Symbol *sym = pnodeFnc->sxFnc.pnodeName->sxVar.sym;

    if (sym != nullptr && !sym->GetFuncExpr())
    {
        if (sym->GetIsGlobal())
        {
            Js::PropertyId propertyId = sym->GetPosition();
            byteCodeGenerator->EmitGlobalFncDeclInit(pnodeFnc->location, propertyId, funcInfoParent);
            if (byteCodeGenerator->GetScriptContext()->GetConfig()->IsBlockScopeEnabled() &&
                byteCodeGenerator->GetFlags() & fscrEval && !funcInfoParent->GetIsStrictMode())
            {
                byteCodeGenerator->EmitPropStore(pnodeFnc->location, sym, nullptr, funcInfoParent);
            }
        }
        else
        {
            if (sym->NeedsSlotAlloc(funcInfoParent))
            {
                if (!sym->GetHasNonCommittedReference() ||
                    (funcInfoParent->GetParsedFunctionBody()->DoStackNestedFunc()))
                {
                    // No point in trying to optimize if there are no references before we have to commit to slot.
                    // And not safe to delay putting a stack function in the slot, since we may miss boxing.
                    sym->SetIsCommittedToSlot();
                }
            }

            byteCodeGenerator->EmitLocalPropInit(pnodeFnc->location, sym, funcInfoParent);
            Symbol * fncScopeSym = sym->GetFuncScopeVarSym();

            if (fncScopeSym)
            {
                Assert(byteCodeGenerator->GetScriptContext()->GetConfig()->IsBlockScopeEnabled());

                if (fncScopeSym->GetIsGlobal() && byteCodeGenerator->GetFlags() & fscrEval)
                {
                    Js::PropertyId propertyId = fncScopeSym->GetPosition();
                    byteCodeGenerator->EmitGlobalFncDeclInit(pnodeFnc->location, propertyId, funcInfoParent);
                }
                else
                {
                    byteCodeGenerator->EmitPropStore(pnodeFnc->location, fncScopeSym, nullptr, funcInfoParent, false, false, /* isFncDeclVar */true);
                }
            }
        }
    }
}

Js::RegSlot ByteCodeGenerator::DefineOneFunction(ParseNode *pnodeFnc, FuncInfo *funcInfoParent, bool generateAssignment, Js::RegSlot regEnv, Js::RegSlot frameDisplayTemp)
{
    Assert(pnodeFnc->nop == knopFncDecl);

    funcInfoParent->AcquireLoc(pnodeFnc);

    if (regEnv == Js::Constants::NoRegister)
    {
        // If the child needs a closure, find a heap-allocated frame to pass to it.
        if (frameDisplayTemp != Js::Constants::NoRegister)
        {
            // We allocated a temp to hold a local frame display value. Use that.
            // It's likely that the FD is on the stack, and we used the temp to load it back.
            regEnv = frameDisplayTemp;
        }
        else if (funcInfoParent->frameDisplayRegister != Js::Constants::NoRegister)
        {
            // This function has built a frame display, so pass it down.
            regEnv = funcInfoParent->frameDisplayRegister;
        }
        else
        {
            // This function has no captured locals but inherits a closure environment, so pass it down.
            regEnv = funcInfoParent->GetEnvRegister();
        }

        regEnv = this->PrependLocalScopes(regEnv, Js::Constants::NoRegister, funcInfoParent);
    }

    // AssertMsg(funcInfo->nonLocalSymbols == 0 || regEnv != funcInfoParent->nullConstantRegister,
    // "We need a closure for the nested function");

    if (regEnv == funcInfoParent->frameDisplayRegister || regEnv == funcInfoParent->GetEnvRegister())
    {
        m_writer.NewFunction(pnodeFnc->location, pnodeFnc->sxFnc.nestedIndex, pnodeFnc->sxFnc.IsGenerator());
    }
    else
    {
        m_writer.NewInnerFunction(pnodeFnc->location, pnodeFnc->sxFnc.nestedIndex, regEnv, pnodeFnc->sxFnc.IsGenerator());
    }

    if (funcInfoParent->IsGlobalFunction() && (this->flags & fscrEval))
    {
        // A function declared at global scope in eval is untrackable,
        // so make sure the caller's cached scope is invalidated.
        this->funcEscapes = true;
    }
    else
    {
        if (pnodeFnc->sxFnc.IsDeclaration())
        {
            Symbol * funcSymbol = pnodeFnc->sxFnc.GetFuncSymbol();
            if (funcSymbol)
            {
                // In the case where a let/const declaration is the same symbol name
                // as the function declaration (shadowing case), the let/const var and
                // the function declaration symbol are the same and share the same flags
                // (particularly, sym->GetIsBlockVar() for this code path).
                //
                // For example:
                // let a = 0;       // <-- sym->GetIsBlockVar() = true
                // function b(){}   // <-- sym2->GetIsBlockVar() = false
                //
                // let x = 0;       // <-- sym3->GetIsBlockVar() = true
                // function x(){}   // <-- sym3->GetIsBlockVar() = true
                //
                // In order to tell if the function is actually part
                // of a block scope, we compare against the function scope here.
                // Note that having a function with the same name as a let/const declaration
                // is a redeclaration error, but we're pushing the fix for this out since it's
                // a bit involved.
                //
                // TODO: Once the redeclaration error is in place, this boolean can be replaced
                // by funcSymbol->GetIsBlockVar().
                Assert(funcInfoParent->GetBodyScope() != nullptr && funcSymbol->GetScope() != nullptr);
                bool isFunctionDeclarationInBlock = funcSymbol->GetScope() != funcInfoParent->GetBodyScope();

                // Track all vars/lets/consts register slot function declarations.
                if (ShouldTrackDebuggerMetadata()
                    // If this is a let binding function declaration at global level, we want to
                    // be sure to track the register location as well.
                    && !(funcInfoParent->IsGlobalFunction() && !isFunctionDeclarationInBlock))
                {
                    if (!funcSymbol->IsInSlot(funcInfoParent))
                    {
                        funcInfoParent->byteCodeFunction->GetFunctionBody()->InsertSymbolToRegSlotList(funcSymbol->GetName(), pnodeFnc->location, funcInfoParent->varRegsCount);
                    }
                }

                if (isFunctionDeclarationInBlock)
                {
                    // We only track inner let bindings for the debugger side.
                    this->TrackFunctionDeclarationPropertyForDebugger(funcSymbol, funcInfoParent);
                }
            }
        }
    }

    if (pnodeFnc->sxFnc.pnodeName == nullptr || !generateAssignment)
    {
        return regEnv;
    }

    EmitAssignmentToFuncName(pnodeFnc, this, funcInfoParent);

    return regEnv;
}

void ByteCodeGenerator::DefineUserVars(FuncInfo *funcInfo)
{
    // Initialize scope-wide variables on entry to the scope. TODO: optimize by detecting uses that are always reached
    // by an existing initialization.

    BOOL fGlobal = funcInfo->IsGlobalFunction();
    ParseNode *pnode;
    Js::FunctionBody *byteCodeFunction = funcInfo->GetParsedFunctionBody();
    // Global declarations need a temp register to hold the init value, but the node shouldn't get a register.
    // Just assign one on the fly and re-use it for all initializations.
    Js::RegSlot tmpReg = fGlobal ? funcInfo->AcquireTmpRegister() : Js::Constants::NoRegister;

    for (pnode = funcInfo->root->sxFnc.pnodeVars; pnode; pnode = pnode->sxVar.pnodeNext)
    {
        Symbol* sym = pnode->sxVar.sym;

        if (sym != nullptr && !(pnode->sxVar.isBlockScopeFncDeclVar && sym->GetIsBlockVar()))
        {
            if (sym->GetIsCatch() || (pnode->nop == knopVarDecl && sym->GetIsBlockVar()))
            {
                // The init node was bound to the catch object, because it's inside a catch and has the
                // same name as the catch object. But we want to define a user var at function scope,
                // so find the right symbol. (We'll still assign the RHS value to the catch object symbol.)
                // This also applies to a var declaration in the same scope as a let declaration.
#if DBG
                if (!sym->GetIsCatch())
                {
                    // Assert that catch cannot be at function scope and let and var at function scope is redeclaration error.
                    Assert(funcInfo->bodyScope != sym->GetScope() || !this->scriptContext->GetConfig()->IsBlockScopeEnabled());
                }
#endif
                sym = funcInfo->bodyScope->FindLocalSymbol(sym->GetName());
                Assert(sym && !sym->GetIsCatch() && !sym->GetIsBlockVar());
            }

            if (sym->GetSymbolType() == STVariable)
            {
                if (fGlobal)
                {
                    Js::PropertyId propertyId = sym->EnsurePosition(this);
                    // We do need to initialize some globals to avoid JS errors on loading undefined variables.
                    // But we first need to make sure we're not trashing built-ins.

                    if (this->flags & fscrEval)
                    {
                        if (funcInfo->byteCodeFunction->GetIsStrictMode())
                        {
                            // Check/Init the property of the frame object
                            this->m_writer.ElementRootU(Js::OpCode::LdLocalElemUndef,
                                funcInfo->FindOrAddReferencedPropertyId(propertyId));
                        }
                        else
                        {
                            // The check and the init involve the first element in the scope chain.
                            this->m_writer.ElementScopedU(
                                Js::OpCode::LdElemUndefScoped, funcInfo->FindOrAddReferencedPropertyId(propertyId));
                        }
                    }
                    else
                    {
                        this->m_writer.ElementU(Js::OpCode::LdElemUndef, ByteCodeGenerator::RootObjectRegister,
                            funcInfo->FindOrAddReferencedPropertyId(propertyId));
                    }
                }
                else if (!sym->GetIsArguments())
                {
                    if (sym->NeedsSlotAlloc(funcInfo))
                    {
                        if (!sym->GetHasNonCommittedReference() ||
                            (sym->GetHasFuncAssignment() && funcInfo->GetParsedFunctionBody()->DoStackNestedFunc()))
                        {
                            // No point in trying to optimize if there are no references before we have to commit to slot.
                            // And not safe to delay putting a stack function in the slot, since we may miss boxing.
                            sym->SetIsCommittedToSlot();
                        }
                    }

                    if ((!sym->GetHasInit() && !sym->IsInSlot(funcInfo)) ||
                        (funcInfo->bodyScope->GetIsObject() && !funcInfo->GetHasCachedScope()))
                    {
                        Js::RegSlot reg = sym->GetLocation();
                        if (reg == Js::Constants::NoRegister)
                        {
                            Assert(sym->IsInSlot(funcInfo));
                            reg = funcInfo->AcquireTmpRegister();
                        }
                        this->m_writer.Reg1(Js::OpCode::LdUndef, reg);
                        this->EmitLocalPropInit(reg, sym, funcInfo);

                        if (ShouldTrackDebuggerMetadata() && !sym->GetHasInit() && !sym->IsInSlot(funcInfo))
                        {
                            byteCodeFunction->InsertSymbolToRegSlotList(sym->GetName(), reg, funcInfo->varRegsCount);
                        }

                        funcInfo->ReleaseTmpRegister(reg);
                    }
                }
                else if (ShouldTrackDebuggerMetadata())
                {
                    if (!sym->GetHasInit() && !sym->IsInSlot(funcInfo))
                    {
                        Js::RegSlot reg = sym->GetLocation();
                        if (reg != Js::Constants::NoRegister)
                        {
                            byteCodeFunction->InsertSymbolToRegSlotList(sym->GetName(), reg, funcInfo->varRegsCount);
                        }
                    }
                }
                sym->SetHasInit(TRUE);
            }
        }

    }
    if (tmpReg != Js::Constants::NoRegister)
    {
        funcInfo->ReleaseTmpRegister(tmpReg);
    }

    for (int i = 0; i < funcInfo->nonUserNonTempRegistersToInitialize.Count(); ++i)
    {
        m_writer.Reg1(Js::OpCode::LdUndef, funcInfo->nonUserNonTempRegistersToInitialize.Item(i));
    }

    this->InitBlockScopedNonTemps(funcInfo->root->sxFnc.pnodeScopes, funcInfo);
}

void ByteCodeGenerator::InitBlockScopedNonTemps(ParseNode *pnode, FuncInfo *funcInfo)
{
    // Initialize all non-temp register variables on entry to the enclosing func - in particular,
    // those with lifetimes that begin after the start of user code and may not be initialized normally.
    // This protects us from, for instance, trying to restore garbage on bailout.
    // It was originally done in debugger mode only, but we do it always to avoid issues with boxing
    // garbage on exit from jitted loop bodies.
    while (pnode)
    {
        switch (pnode->nop)
        {
        case knopFncDecl:
            if (this->scriptContext->GetConfig()->IsBlockScopeEnabled())
            {
                // If this is a block-scoped function, initialize it.
                ParseNode *pnodeName = pnode->sxFnc.pnodeName;
                if (!pnode->sxFnc.IsMethod() && pnodeName && pnodeName->nop == knopVarDecl)
                {
                    Symbol *sym = pnodeName->sxVar.sym;
                    Assert(sym);
                    if (sym->GetLocation() != Js::Constants::NoRegister &&
                        sym->GetScope()->IsBlockScope(funcInfo) &&
                        sym->GetScope()->GetFunc() == funcInfo)
                    {
                        this->m_writer.Reg1(Js::OpCode::LdUndef, sym->GetLocation());
                    }
                }
            }
            // No need to recurse to the nested scopes, as they belong to a nested function.
            pnode = pnode->sxFnc.pnodeNext;
            break;

        case knopBlock:
        {
            Scope *scope = pnode->sxBlock.scope;
            if (scope)
            {
                if (scope->IsBlockScope(funcInfo))
                {
                    Js::RegSlot scopeLoc = scope->GetLocation();
                    if (scopeLoc != Js::Constants::NoRegister && !funcInfo->IsTmpReg(scopeLoc))
                    {
                        this->m_writer.Reg1(Js::OpCode::LdUndef, scopeLoc);
                    }
                }
                auto fnInit = [this, funcInfo](ParseNode *pnode)
                {
                    Symbol *sym = pnode->sxVar.sym;
                    if (!sym->IsInSlot(funcInfo) && !sym->GetIsGlobal())
                    {
                        this->m_writer.Reg1(Js::OpCode::InitUndecl, pnode->sxVar.sym->GetLocation());
                    }
                };
                IterateBlockScopedVariables(pnode, fnInit);
            }
            InitBlockScopedNonTemps(pnode->sxBlock.pnodeScopes, funcInfo);
            pnode = pnode->sxBlock.pnodeNext;
            break;
        }
        case knopCatch:
            InitBlockScopedNonTemps(pnode->sxCatch.pnodeScopes, funcInfo);
            pnode = pnode->sxCatch.pnodeNext;
            break;

        case knopWith:
        {
            Js::RegSlot withLoc = pnode->location;
            AssertMsg(withLoc != Js::Constants::NoRegister && !funcInfo->IsTmpReg(withLoc),
                "We should put with objects at known stack locations in debug mode");
            this->m_writer.Reg1(Js::OpCode::LdUndef, withLoc);
            InitBlockScopedNonTemps(pnode->sxWith.pnodeScopes, funcInfo);
            pnode = pnode->sxWith.pnodeNext;
            break;
        }

        default:
            Assert(false);
            return;
        }
    }
}

void ByteCodeGenerator::EmitScopeObjectInit(FuncInfo *funcInfo)
{
    Assert(!funcInfo->byteCodeFunction->GetFunctionBody()->DoStackNestedFunc());

    if (!funcInfo->GetHasCachedScope() /* || forcing scope/inner func caching */)
    {
        return;
    }

    uint slotCount = funcInfo->bodyScope->GetScopeSlotCount();
    uint cachedFuncCount = 0;
    Js::PropertyId firstFuncSlot = Js::Constants::NoProperty;
    Js::PropertyId firstVarSlot = Js::Constants::NoProperty;
    uint extraAlloc = (slotCount + Js::ActivationObjectEx::ExtraSlotCount()) * sizeof(Js::PropertyId);

    // Create and fill the array of local property ID's.
    // They all have slots assigned to them already (if they need them): see StartEmitFunction.

    Js::PropertyIdArray *propIds = AnewPlus(alloc, extraAlloc, Js::PropertyIdArray, slotCount);

    ParseNode *pnodeFnc = funcInfo->root;
    ParseNode *pnode;
    Symbol *sym;

    if (funcInfo->GetFuncExprNameReference() && pnodeFnc->sxFnc.GetFuncSymbol()->GetScope() == funcInfo->GetBodyScope())
    {
        Symbol::SaveToPropIdArray(pnodeFnc->sxFnc.GetFuncSymbol(), propIds, this);
    }

    if (funcInfo->GetHasArguments())
    {
        // Because the arguments object can access all instances of same-named formals ("function(x,x){...}"),
        // be sure we initialize any duplicate appearances of a formal parameter to "NoProperty".
        Js::PropertyId slot = 0;
        auto initArg = [&](ParseNode *pnode)
        {
            if (pnode->IsVarLetOrConst())
            {
                Symbol *sym = pnode->sxVar.sym;
                Assert(sym);
                if (sym->GetScopeSlot() == slot)
                {
                    // This is the last appearance of the formal, so record the ID.
                    Symbol::SaveToPropIdArray(sym, propIds, this);
                }
                else
                {
                    // This is an earlier duplicate appearance of the formal, so use NoProperty as a placeholder
                    // since this slot can't be accessed by name.
                    Assert(sym->GetScopeSlot() != Js::Constants::NoProperty && sym->GetScopeSlot() > slot);
                    propIds->elements[slot] = Js::Constants::NoProperty;
                }
                slot++;
            }
        };
        MapFormalsWithoutRest(pnodeFnc, initArg);

        // initArg assumes the sym is in a slot, but this may not be true for the rest parameter.
        if (pnodeFnc->sxFnc.pnodeRest != nullptr && pnodeFnc->sxFnc.pnodeRest->sxVar.sym->IsInSlot(funcInfo))
        {
            initArg(pnodeFnc->sxFnc.pnodeRest);
        }
    }
    else
    {
        MapFormals(pnodeFnc, [&](ParseNode *pnode)
        {
            if (pnode->IsVarLetOrConst())
            {
                Symbol::SaveToPropIdArray(pnode->sxVar.sym, propIds, this);
            }
        });
    }

    auto saveFunctionVarsToPropIdArray = [&](ParseNode *pnodeFunction)
    {
        if (pnodeFunction->sxFnc.IsDeclaration())
        {
            ParseNode *pnodeName = pnodeFunction->sxFnc.pnodeName;
            if (pnodeName != nullptr)
            {
                while (pnodeName->nop == knopList)
                {
                    if (pnodeName->sxBin.pnode1->nop == knopVarDecl)
                    {
                        sym = pnodeName->sxBin.pnode1->sxVar.sym;
                        if (sym)
                        {
                            Symbol::SaveToPropIdArray(sym, propIds, this, &firstFuncSlot);
                        }
                    }
                    pnodeName = pnodeName->sxBin.pnode2;
                }
                if (pnodeName->nop == knopVarDecl)
                {
                    sym = pnodeName->sxVar.sym;
                    if (sym)
                    {
                        Symbol::SaveToPropIdArray(sym, propIds, this, &firstFuncSlot);
                        cachedFuncCount++;
                    }
                }
            }
        }
    };
    MapContainerScopeFunctions(pnodeFnc, saveFunctionVarsToPropIdArray);

    for (pnode = pnodeFnc->sxFnc.pnodeVars; pnode; pnode = pnode->sxVar.pnodeNext)
    {
        sym = pnode->sxVar.sym;
        if (!(pnode->sxVar.isBlockScopeFncDeclVar && sym->GetIsBlockVar()))
        {
            if (sym->GetIsCatch() || (pnode->nop == knopVarDecl && sym->GetIsBlockVar()))
            {
                sym = funcInfo->bodyScope->FindLocalSymbol(sym->GetName());
            }
            Symbol::SaveToPropIdArray(sym, propIds, this, &firstVarSlot);
        }
    }

    ParseNode *pnodeBlock = pnodeFnc->sxFnc.pnodeScopes;
    for (pnode = pnodeBlock->sxBlock.pnodeLexVars; pnode; pnode = pnode->sxVar.pnodeNext)
    {
        sym = pnode->sxVar.sym;
        Symbol::SaveToPropIdArray(sym, propIds, this, &firstVarSlot);
    }

    pnodeBlock = pnodeFnc->sxFnc.pnodeBodyScope;
    for (pnode = pnodeBlock->sxBlock.pnodeLexVars; pnode; pnode = pnode->sxVar.pnodeNext)
    {
        sym = pnode->sxVar.sym;
        Symbol::SaveToPropIdArray(sym, propIds, this, &firstVarSlot);
    }

    if (funcInfo->thisScopeSlot != Js::Constants::NoRegister)
    {
        propIds->elements[funcInfo->thisScopeSlot] = Js::PropertyIds::_lexicalThisSlotSymbol;
    }
    if (funcInfo->newTargetScopeSlot != Js::Constants::NoRegister)
    {
        propIds->elements[funcInfo->newTargetScopeSlot] = Js::PropertyIds::_lexicalNewTargetSymbol;
    }
    if (funcInfo->superScopeSlot != Js::Constants::NoRegister)
    {
        propIds->elements[funcInfo->superScopeSlot] = Js::PropertyIds::_superReferenceSymbol;
    }
    if (funcInfo->superCtorScopeSlot != Js::Constants::NoRegister)
    {
        propIds->elements[funcInfo->superCtorScopeSlot] = Js::PropertyIds::_superCtorReferenceSymbol;
    }

    // Write the first func slot and first var slot into the auxiliary data
    Js::PropertyId *slots = propIds->elements + slotCount;
    slots[0] = cachedFuncCount;
    slots[1] = firstFuncSlot;
    slots[2] = firstVarSlot;
    slots[3] = funcInfo->GetParsedFunctionBody()->NewObjectLiteral();

    propIds->hasNonSimpleParams = !funcInfo->root->sxFnc.IsSimpleParameterList();
    funcInfo->localPropIdOffset = m_writer.InsertAuxiliaryData(propIds, sizeof(Js::PropertyIdArray) + extraAlloc);
    Assert(funcInfo->localPropIdOffset == 0);
    funcInfo->GetParsedFunctionBody()->SetHasCachedScopePropIds(true);

    AdeletePlus(alloc, extraAlloc, propIds);
}

void ByteCodeGenerator::FinalizeRegisters(FuncInfo * funcInfo, Js::FunctionBody * byteCodeFunction)
{
    if (funcInfo->NeedEnvRegister())
    {
        bool constReg = !funcInfo->GetIsEventHandler() && funcInfo->IsGlobalFunction() && !(this->flags & fscrEval);
        funcInfo->AssignEnvRegister(constReg);
    }

    // Set the function body's constant count before emitting anything so that the byte code writer
    // can distinguish constants from variables.
    byteCodeFunction->SetConstantCount(funcInfo->constRegsCount);

    if (funcInfo->frameDisplayRegister != Js::Constants::NoRegister)
    {
        byteCodeFunction->SetLocalFrameDisplayReg(funcInfo->frameDisplayRegister);
    }

    if (funcInfo->frameObjRegister != Js::Constants::NoRegister)
    {
        byteCodeFunction->SetLocalClosureReg(funcInfo->frameObjRegister);
        byteCodeFunction->SetHasScopeObject(true);
    }
    else if (funcInfo->frameSlotsRegister != Js::Constants::NoRegister)
    {
        byteCodeFunction->SetLocalClosureReg(funcInfo->frameSlotsRegister);
    }

    if (this->IsInDebugMode())
    {
        // Give permanent registers to the inner scopes in debug mode.
        uint innerScopeCount = funcInfo->InnerScopeCount();
        byteCodeFunction->SetInnerScopeCount(innerScopeCount);
        if (innerScopeCount)
        {
            funcInfo->SetFirstInnerScopeReg(funcInfo->NextVarRegister());
            for (uint i = 1; i < innerScopeCount; i++)
            {
                funcInfo->NextVarRegister();
            }
        }
    }

    // NOTE: The FB expects the yield reg to be the final non-temp.
    if (byteCodeFunction->IsGenerator())
    {
        funcInfo->AssignYieldRegister();
    }

    Js::RegSlot firstTmpReg = funcInfo->varRegsCount;
    funcInfo->SetFirstTmpReg(firstTmpReg);
    byteCodeFunction->SetFirstTmpReg(funcInfo->RegCount());
}

void ByteCodeGenerator::InitScopeSlotArray(FuncInfo * funcInfo)
{
    // Record slots info for ScopeSlots/ScopeObject.
    uint scopeSlotCount = funcInfo->bodyScope->GetScopeSlotCount();
    if (scopeSlotCount == 0)
    {
        return;
    }

    Js::FunctionBody *byteCodeFunction = funcInfo->GetParsedFunctionBody();
    Js::PropertyId *propertyIdsForScopeSlotArray = RecyclerNewArrayLeafZ(scriptContext->GetRecycler(), Js::PropertyId, scopeSlotCount);
    AssertMsg(!byteCodeFunction->IsReparsed() || byteCodeFunction->m_wasEverAsmjsMode || byteCodeFunction->scopeSlotArraySize == scopeSlotCount,
        "The slot array size is different between debug and non-debug mode");
    byteCodeFunction->SetPropertyIdsForScopeSlotArray(propertyIdsForScopeSlotArray, scopeSlotCount);
#if DEBUG
    for (UINT i = 0; i < scopeSlotCount; i++)
    {
        propertyIdsForScopeSlotArray[i] = Js::Constants::NoProperty;
    }
#endif

    auto setPropIdsForScopeSlotArray = [funcInfo, propertyIdsForScopeSlotArray](Symbol *const sym)
    {
        if (sym->NeedsSlotAlloc(funcInfo))
        {
            // All properties should get correct propertyId here.
            Assert(sym->HasScopeSlot()); // We can't allocate scope slot now. Any symbol needing scope slot must have allocated it before this point.
            propertyIdsForScopeSlotArray[sym->GetScopeSlot()] = sym->EnsurePosition(funcInfo);
        }
    };
    if (funcInfo->GetParamScope() != nullptr)
    {
        funcInfo->GetParamScope()->ForEachSymbol(setPropIdsForScopeSlotArray);
    }
    funcInfo->GetBodyScope()->ForEachSymbol(setPropIdsForScopeSlotArray);

    if (funcInfo->thisScopeSlot != Js::Constants::NoRegister)
    {
        propertyIdsForScopeSlotArray[funcInfo->thisScopeSlot] = Js::PropertyIds::_lexicalThisSlotSymbol;
    }

    if (funcInfo->newTargetScopeSlot != Js::Constants::NoRegister)
    {
        propertyIdsForScopeSlotArray[funcInfo->newTargetScopeSlot] = Js::PropertyIds::_lexicalNewTargetSymbol;
    }

    if (funcInfo->superScopeSlot != Js::Constants::NoRegister)
    {
        propertyIdsForScopeSlotArray[funcInfo->superScopeSlot] = Js::PropertyIds::_superReferenceSymbol;
    }

    if (funcInfo->superCtorScopeSlot != Js::Constants::NoRegister)
    {
        propertyIdsForScopeSlotArray[funcInfo->superCtorScopeSlot] = Js::PropertyIds::_superCtorReferenceSymbol;
    }

#if DEBUG
    for (UINT i = 0; i < scopeSlotCount; i++)
    {
        Assert(propertyIdsForScopeSlotArray[i] != Js::Constants::NoProperty
            || funcInfo->frameObjRegister != Js::Constants::NoRegister); // ScopeObject may have unassigned entries, e.g. for same-named parameters
    }
#endif
}

// temporarily load all constants and special registers in a single block
void ByteCodeGenerator::LoadAllConstants(FuncInfo *funcInfo)
{
    Symbol *sym;

    Js::FunctionBody *byteCodeFunction = funcInfo->GetParsedFunctionBody();
    byteCodeFunction->CreateConstantTable();

    if (funcInfo->nullConstantRegister != Js::Constants::NoRegister)
    {
        byteCodeFunction->RecordNullObject(byteCodeFunction->MapRegSlot(funcInfo->nullConstantRegister));
    }

    if (funcInfo->undefinedConstantRegister != Js::Constants::NoRegister)
    {
        byteCodeFunction->RecordUndefinedObject(byteCodeFunction->MapRegSlot(funcInfo->undefinedConstantRegister));
    }

    if (funcInfo->trueConstantRegister != Js::Constants::NoRegister)
    {
        byteCodeFunction->RecordTrueObject(byteCodeFunction->MapRegSlot(funcInfo->trueConstantRegister));
    }

    if (funcInfo->falseConstantRegister != Js::Constants::NoRegister)
    {
        byteCodeFunction->RecordFalseObject(byteCodeFunction->MapRegSlot(funcInfo->falseConstantRegister));
    }

    if (funcInfo->frameObjRegister != Js::Constants::NoRegister)
    {
        m_writer.RecordObjectRegister(funcInfo->frameObjRegister);
        if (!funcInfo->GetApplyEnclosesArgs())
        {
            this->EmitScopeObjectInit(funcInfo);
        }

#if DBG
        uint count = 0;
        funcInfo->GetBodyScope()->ForEachSymbol([&](Symbol *const sym)
        {
            if (sym->NeedsSlotAlloc(funcInfo))
            {
                // All properties should get correct propertyId here.
                count++;
            }
        });

        if (funcInfo->GetParamScope() != nullptr)
        {
            funcInfo->GetParamScope()->ForEachSymbol([&](Symbol *const sym)
            {
                if (sym->NeedsSlotAlloc(funcInfo))
                {
                    // All properties should get correct propertyId here.
                    count++;
                }
            });
        }

        // A reparse should result in the same size of the activation object.
        // Exclude functions which were created from the ByteCodeCache.
        AssertMsg(!byteCodeFunction->IsReparsed() || byteCodeFunction->HasGeneratedFromByteCodeCache() ||
            byteCodeFunction->scopeObjectSize == count || byteCodeFunction->m_wasEverAsmjsMode,
            "The activation object size is different between debug and non-debug mode");
        byteCodeFunction->scopeObjectSize = count;
#endif
    }
    else if (funcInfo->frameSlotsRegister != Js::Constants::NoRegister)
    {
        int scopeSlotCount = funcInfo->bodyScope->GetScopeSlotCount();
        if (scopeSlotCount == 0)
        {
            AssertMsg(funcInfo->frameDisplayRegister != Js::Constants::NoRegister, "Why do we need scope slots?");
            m_writer.Reg1(Js::OpCode::LdC_A_Null, funcInfo->frameSlotsRegister);
        }
    }

    if (funcInfo->funcExprScope && funcInfo->funcExprScope->GetIsObject())
    {
        byteCodeFunction->SetFuncExprScopeReg(funcInfo->funcExprScope->GetLocation());
        byteCodeFunction->SetEnvDepth((uint16)-1);
    }

    bool thisLoadedFromParams = false;

    if (funcInfo->NeedEnvRegister())
    {
        byteCodeFunction->SetEnvReg(funcInfo->GetEnvRegister());
        if (funcInfo->GetIsEventHandler())
        {
            byteCodeFunction->SetThisRegForEventHandler(funcInfo->thisPointerRegister);
            // The environment is the namespace hierarchy starting with "this".
            Assert(!funcInfo->RegIsConst(funcInfo->GetEnvRegister()));
            thisLoadedFromParams = true;
            this->InvalidateCachedOuterScopes(funcInfo);
        }
        else if (funcInfo->IsGlobalFunction() && !(this->flags & fscrEval))
        {
            Assert(funcInfo->RegIsConst(funcInfo->GetEnvRegister()));

            if (funcInfo->GetIsStrictMode())
            {
                byteCodeFunction->RecordStrictNullDisplayConstant(byteCodeFunction->MapRegSlot(funcInfo->GetEnvRegister()));
            }
            else
            {
                byteCodeFunction->RecordNullDisplayConstant(byteCodeFunction->MapRegSlot(funcInfo->GetEnvRegister()));
            }
        }
        else
        {
            // environment may be required to load "this"
            Assert(!funcInfo->RegIsConst(funcInfo->GetEnvRegister()));
            this->InvalidateCachedOuterScopes(funcInfo);
        }
    }

    if (funcInfo->frameDisplayRegister != Js::Constants::NoRegister)
    {
        m_writer.RecordFrameDisplayRegister(funcInfo->frameDisplayRegister);
    }

    // new.target may be used to construct the 'this' register so make sure to load it first
    if (funcInfo->newTargetRegister != Js::Constants::NoRegister)
    {
        this->LoadNewTargetObject(funcInfo);
    }

    if (funcInfo->thisPointerRegister != Js::Constants::NoRegister)
    {
        this->LoadThisObject(funcInfo, thisLoadedFromParams);
    }

    this->RecordAllIntConstants(funcInfo);
    this->RecordAllStrConstants(funcInfo);
    this->RecordAllStringTemplateCallsiteConstants(funcInfo);

    funcInfo->doubleConstantToRegister.Map([byteCodeFunction](double d, Js::RegSlot location)
    {
        byteCodeFunction->RecordFloatConstant(byteCodeFunction->MapRegSlot(location), d);
    });

    if (funcInfo->GetHasArguments())
    {
        sym = funcInfo->GetArgumentsSymbol();
        Assert(sym);
        Assert(funcInfo->GetHasHeapArguments());

        if (funcInfo->GetCallsEval() || (!funcInfo->GetApplyEnclosesArgs()))
        {
            this->LoadHeapArguments(funcInfo);
        }

    }
    else if (!funcInfo->IsGlobalFunction() && !IsInNonDebugMode())
    {
        uint count = funcInfo->inArgsCount + (funcInfo->root->sxFnc.pnodeRest != nullptr ? 1 : 0) - 1;
        if (count != 0)
        {
            Js::PropertyIdArray *propIds = RecyclerNewPlus(scriptContext->GetRecycler(), count * sizeof(Js::PropertyId), Js::PropertyIdArray, count);

            GetFormalArgsArray(this, funcInfo, propIds);
            byteCodeFunction->SetPropertyIdsOfFormals(propIds);
        }
    }

    //
    // If the function is a function expression with a name,
    // load the function object at runtime to its activation object.
    //
    sym = funcInfo->root->sxFnc.GetFuncSymbol();
    bool funcExprWithName = !funcInfo->IsGlobalFunction() && sym && sym->GetFuncExpr();

    if (funcExprWithName)
    {
        if (funcInfo->GetFuncExprNameReference() ||
            (funcInfo->funcExprScope && funcInfo->funcExprScope->GetIsObject()))
        {
            //
            // x = function f(...) { ... }
            // A named function expression's name (Symbol:f) belongs to the enclosing scope.
            // Thus there are no uses of 'f' within the scope of the function (as references to 'f'
            // are looked up in the closure). So, we can't use f's register as it is from the enclosing
            // scope's register namespace. So use a tmp register.
            // In ES5 mode though 'f' is *not* a part of the enclosing scope. So we always assign 'f' a register
            // from it's register namespace, which LdFuncExpr can use.
            //
            Js::RegSlot ldFuncExprDst = sym->GetLocation();
            this->m_writer.Reg1(Js::OpCode::LdFuncExpr, ldFuncExprDst);

            if (sym->IsInSlot(funcInfo))
            {
                Assert(!this->TopFuncInfo()->GetParsedFunctionBody()->DoStackNestedFunc());
                Js::RegSlot scopeLocation;
                AnalysisAssert(funcInfo->funcExprScope);

                if (funcInfo->funcExprScope->GetIsObject())
                {
                    scopeLocation = funcInfo->funcExprScope->GetLocation();
                    this->m_writer.Property(Js::OpCode::StFuncExpr, sym->GetLocation(), scopeLocation,
                                            funcInfo->FindOrAddReferencedPropertyId(sym->GetPosition()));
                }
                else
                {
                    this->m_writer.ElementU(Js::OpCode::StLocalFuncExpr, sym->GetLocation(),
                                            funcInfo->FindOrAddReferencedPropertyId(sym->GetPosition()));
                }
            }
            else if (ShouldTrackDebuggerMetadata())
            {
                funcInfo->byteCodeFunction->GetFunctionBody()->InsertSymbolToRegSlotList(sym->GetName(), sym->GetLocation(), funcInfo->varRegsCount);
            }
        }
    }
}

void ByteCodeGenerator::InvalidateCachedOuterScopes(FuncInfo *funcInfo)
{
    Assert(funcInfo->GetEnvRegister() != Js::Constants::NoRegister);

    // Walk the scope stack, from funcInfo outward, looking for scopes that have been cached.

    Scope *scope = funcInfo->GetBodyScope()->GetEnclosingScope();
    uint32 envIndex = 0;

    while (scope && scope->GetFunc() == funcInfo)
    {
        // Skip over FuncExpr Scope and parameter scope for current funcInfo to get to the first enclosing scope of the outer function.
        scope = scope->GetEnclosingScope();
    }

    for (; scope; scope = scope->GetEnclosingScope())
    {
        FuncInfo *func = scope->GetFunc();
        if (scope == func->GetBodyScope())
        {
            if (func->Escapes() && func->GetHasCachedScope())
            {
                Assert(scope->GetIsObject());
                this->m_writer.Unsigned1(Js::OpCode::InvalCachedScope, envIndex);
            }
        }
        if (scope->GetMustInstantiate())
        {
            envIndex++;
        }
    }
}

void ByteCodeGenerator::LoadThisObject(FuncInfo *funcInfo, bool thisLoadedFromParams)
{
    if (this->scriptContext->GetConfig()->IsES6ClassAndExtendsEnabled() && funcInfo->IsClassConstructor())
    {
        // Derived class constructors initialize 'this' to be Undecl
        //   - we'll check this value during a super call and during 'this' access
        // Base class constructors initialize 'this' to a new object using new.target
        if (funcInfo->IsBaseClassConstructor())
        {
            EmitBaseClassConstructorThisObject(funcInfo);
        }
        else
        {
            this->m_writer.Reg1(Js::OpCode::InitUndecl, funcInfo->thisPointerRegister);
        }
    }
    else if (!funcInfo->IsGlobalFunction() || (this->flags & fscrEval))
    {
        //
        // thisLoadedFromParams would be true for the event Handler case,
        // "this" would have been loaded from parameters to put in the environment
        //
        if (!thisLoadedFromParams && !funcInfo->IsLambda())
        {
            m_writer.ArgIn0(funcInfo->thisPointerRegister);
        }
        if (!(this->flags & fscrEval) || !funcInfo->IsGlobalFunction())
        {
            // we don't want to emit 'this' for eval, because 'this' value in eval is equal to 'this' value of caller
            // and does not depend on "use strict" inside of eval.
            // so we pass 'this' directly in GlobalObject::EntryEval()
            EmitThis(funcInfo, funcInfo->thisPointerRegister);
        }
    }
    else
    {
        Assert(funcInfo->IsGlobalFunction());
        Js::RegSlot root = funcInfo->nullConstantRegister;
        EmitThis(funcInfo, root);
    }
}

void ByteCodeGenerator::LoadNewTargetObject(FuncInfo *funcInfo)
{
    if (funcInfo->IsClassConstructor())
    {
        Assert(!funcInfo->IsLambda());

        m_writer.ArgIn0(funcInfo->newTargetRegister);
    }
    else if (funcInfo->IsLambda() && !(this->flags & fscrEval))
    {
        Scope *scope;
        Js::PropertyId envIndex = -1;
        GetEnclosingNonLambdaScope(funcInfo, scope, envIndex);

        if (scope->GetFunc()->IsGlobalFunction())
        {
            m_writer.Reg1(Js::OpCode::LdUndef, funcInfo->newTargetRegister);
        }
        else
        {
            Js::PropertyId slot = scope->GetFunc()->newTargetScopeSlot;
            EmitInternalScopedSlotLoad(funcInfo, scope, envIndex, slot, funcInfo->newTargetRegister);
        }
    }
    else if (this->flags & fscrEval)
    {
        Js::RegSlot scopeLocation;

        if (funcInfo->byteCodeFunction->GetIsStrictMode() && funcInfo->IsGlobalFunction())
        {
            scopeLocation = funcInfo->frameDisplayRegister;
        }
        else if (funcInfo->NeedEnvRegister())
        {
            scopeLocation = funcInfo->GetEnvRegister();
        }
        else
        {
            // If this eval doesn't have environment register or frame display register, we didn't capture anything from a class constructor.
            m_writer.Reg1(Js::OpCode::LdNewTarget, funcInfo->newTargetRegister);
            return;
        }

        uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, Js::PropertyIds::_lexicalNewTargetSymbol, false, false);
        this->m_writer.ElementP(Js::OpCode::ScopedLdFld, funcInfo->newTargetRegister, cacheId);
    }
    else if (funcInfo->IsGlobalFunction())
    {
        m_writer.Reg1(Js::OpCode::LdUndef, funcInfo->newTargetRegister);
    }
    else
    {
        m_writer.Reg1(Js::OpCode::LdNewTarget, funcInfo->newTargetRegister);
    }
}

void ByteCodeGenerator::EmitScopeSlotLoadThis(FuncInfo *funcInfo, Js::RegSlot regLoc, bool chkUndecl)
{
    FuncInfo* nonLambdaFunc = funcInfo;
    if (funcInfo->IsLambda())
    {
        nonLambdaFunc = FindEnclosingNonLambda();
    }

    if (nonLambdaFunc->IsClassConstructor() && !nonLambdaFunc->IsBaseClassConstructor())
    {
        // If we are in a derived class constructor and we have a scope slot for 'this',
        // we need to load 'this' from the scope slot. This is to support the case where
        // the call to initialize 'this' via super() is inside a lambda since the lambda
        // can't assign to the 'this' register of the parent constructor.
        if (nonLambdaFunc->thisScopeSlot != Js::Constants::NoRegister)
        {
            Js::PropertyId slot = nonLambdaFunc->thisScopeSlot;

            EmitInternalScopedSlotLoad(funcInfo, slot, regLoc, chkUndecl);
        }
        else if (funcInfo->thisPointerRegister != Js::Constants::NoRegister && chkUndecl)
        {
            this->m_writer.Reg1(Js::OpCode::ChkUndecl, funcInfo->thisPointerRegister);
        }
        else if (chkUndecl)
        {
            // If we don't have a scope slot for 'this' we know that super could not have
            // been called inside a lambda so we can check to see if we called
            // super and assigned to the this register already. If not, this should trigger
            // a ReferenceError.
            EmitUseBeforeDeclarationRuntimeError(this, regLoc, false);
        }
    }
    else if (this->flags & fscrEval && (funcInfo->IsGlobalFunction() || (funcInfo->IsLambda() && nonLambdaFunc->IsGlobalFunction()))
        && funcInfo->GetBodyScope()->GetIsObject())
    {
        Js::RegSlot scopeLocation;

        if (funcInfo->byteCodeFunction->GetIsStrictMode() && funcInfo->IsGlobalFunction())
        {
            scopeLocation = funcInfo->frameDisplayRegister;
        }
        else if (funcInfo->NeedEnvRegister())
        {
            scopeLocation = funcInfo->GetEnvRegister();
        }
        else
        {
            // If this eval doesn't have environment register or frame display register, we didn't capture anything from a class constructor
            return;
        }

        // CONSIDER [tawoll] - Should we add a ByteCodeGenerator flag (fscrEvalWithClassConstructorParent) and avoid doing this runtime check?
        Js::ByteCodeLabel skipLabel = this->Writer()->DefineLabel();
        this->Writer()->BrReg1(Js::OpCode::BrNotUndecl_A, skipLabel, funcInfo->thisPointerRegister);

        uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, Js::PropertyIds::_lexicalThisSlotSymbol, false, false);
        this->m_writer.ElementP(Js::OpCode::ScopedLdFld, funcInfo->thisPointerRegister, cacheId);
        if (chkUndecl)
        {
            this->m_writer.Reg1(Js::OpCode::ChkUndecl, funcInfo->thisPointerRegister);
        }

        this->Writer()->MarkLabel(skipLabel);
    }
}

void ByteCodeGenerator::EmitScopeSlotStoreThis(FuncInfo *funcInfo, Js::RegSlot regLoc, bool chkUndecl)
{
    if (this->flags & fscrEval && (funcInfo->IsGlobalFunction() || (funcInfo->IsLambda() && FindEnclosingNonLambda()->IsGlobalFunction())))
    {
        Js::RegSlot scopeLocation;

        if (funcInfo->byteCodeFunction->GetIsStrictMode() && funcInfo->IsGlobalFunction())
        {
            scopeLocation = funcInfo->frameDisplayRegister;
        }
        else
        {
            scopeLocation = funcInfo->GetEnvRegister();
        }

        uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, Js::PropertyIds::_lexicalThisSlotSymbol, false, true);
        this->m_writer.ElementP(GetScopedStFldOpCode(funcInfo->byteCodeFunction->GetIsStrictMode()), funcInfo->thisPointerRegister, cacheId);
    }
    else if (regLoc != Js::Constants::NoRegister)
    {
        EmitInternalScopedSlotStore(funcInfo, regLoc, funcInfo->thisPointerRegister);
    }
}

void ByteCodeGenerator::EmitSuperCall(FuncInfo* funcInfo, ParseNode* pnode, BOOL fReturnValue)
{
    Assert(pnode->sxCall.pnodeTarget->nop == knopSuper);

    FuncInfo* nonLambdaFunc = funcInfo;

    if (funcInfo->IsLambda())
    {
        nonLambdaFunc = this->FindEnclosingNonLambda();
    }

    if (nonLambdaFunc->IsBaseClassConstructor())
    {
        // super() is not allowed in base class constructors. If we detect this, emit a ReferenceError and skip making the call.
        this->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_ClassSuperInBaseClass));
        return;
    }
    else
    {
        EmitSuperFieldPatch(funcInfo, pnode, this);
        pnode->isUsed = true;
    }

    // We already know pnode->sxCall.pnodeTarget->nop is super but we can't use the super register in case
    // this is an eval and we will load super dynamically from the scope using ScopedLdSuper.
    // That means we'll have to rely on the location of the call target to be sure.
    // We have to make sure to allocate the location for the node now, before we try to branch on it.
    Emit(pnode->sxCall.pnodeTarget, this, funcInfo, false, /*isConstructorCall*/ true); // reuse isConstructorCall ("new super()" is illegal)

    //
    // if (super is class constructor) {
    //   _this = new.target;
    // } else {
    //   _this = NewScObjFull(new.target);
    // }
    //
    // temp = super.call(_this, new.target); // CallFlag_New | CallFlag_NewTarget | CallFlag_ExtraArg
    // if (temp is object) {
    //   _this = temp;
    // }
    //
    // if (UndeclBlockVar === this) {
    //   this = _this;
    // } else {
    //   throw ReferenceError;
    // }
    //
    funcInfo->AcquireLoc(pnode);
    Js::RegSlot thisForSuperCall = funcInfo->AcquireTmpRegister();
    Js::ByteCodeLabel useNewTargetForThisLabel = this->Writer()->DefineLabel();
    Js::ByteCodeLabel makeCallLabel = this->Writer()->DefineLabel();
    Js::ByteCodeLabel useSuperCallResultLabel = this->Writer()->DefineLabel();
    Js::ByteCodeLabel doneLabel = this->Writer()->DefineLabel();

    this->Writer()->BrReg1(Js::OpCode::BrOnClassConstructor, useNewTargetForThisLabel, pnode->sxCall.pnodeTarget->location);

    this->Writer()->Reg2(Js::OpCode::NewScObjectNoCtorFull, thisForSuperCall, funcInfo->newTargetRegister);
    this->Writer()->Br(Js::OpCode::Br, makeCallLabel);

    this->Writer()->MarkLabel(useNewTargetForThisLabel);
    this->Writer()->Reg2(Js::OpCode::Ld_A, thisForSuperCall, funcInfo->newTargetRegister);

    this->Writer()->MarkLabel(makeCallLabel);
    EmitCall(pnode, Js::Constants::NoRegister, this, funcInfo, fReturnValue, /*fEvaluateComponents*/ true, /*fHasNewTarget*/ true, thisForSuperCall);

    // We have to use another temp for the this value before assigning to this register.
    // This is because IRBuilder does not expect us to use the value of a temp after potentially assigning to that same temp.
    // Ex:
    // _this = new.target;
    // temp = super.call(_this);
    // if (temp is object) {
    //   _this = temp; // creates a new sym for _this as it was previously used
    // }
    // this = _this; // tries to loads a value from the old sym (which is dead)
    Js::RegSlot valueForThis = funcInfo->AcquireTmpRegister();

    this->Writer()->BrReg1(Js::OpCode::BrOnObject_A, useSuperCallResultLabel, pnode->location);
    this->Writer()->Reg2(Js::OpCode::Ld_A, valueForThis, thisForSuperCall);
    this->Writer()->Br(Js::OpCode::Br, doneLabel);
    this->Writer()->MarkLabel(useSuperCallResultLabel);
    this->Writer()->Reg2(Js::OpCode::Ld_A, valueForThis, pnode->location);
    this->Writer()->MarkLabel(doneLabel);

    // The call is done and we know what we will bind to 'this' so let's check to see if 'this' is already decl.
    // We may need to load 'this' from the scope slot.
    EmitScopeSlotLoadThis(funcInfo, funcInfo->thisPointerRegister, false);

    Js::ByteCodeLabel skipLabel = this->Writer()->DefineLabel();
    Js::RegSlot tmpUndeclReg = funcInfo->AcquireTmpRegister();
    this->Writer()->Reg1(Js::OpCode::InitUndecl, tmpUndeclReg);
    this->Writer()->BrReg2(Js::OpCode::BrSrEq_A, skipLabel, funcInfo->thisPointerRegister, tmpUndeclReg);
    funcInfo->ReleaseTmpRegister(tmpUndeclReg);

    this->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_ClassThisAlreadyAssigned));
    this->Writer()->MarkLabel(skipLabel);

    this->Writer()->Reg2(Js::OpCode::StrictLdThis, funcInfo->thisPointerRegister, valueForThis);
    funcInfo->ReleaseTmpRegister(valueForThis);
    funcInfo->ReleaseTmpRegister(thisForSuperCall);

    // We already assigned the result of super() to the 'this' register but we need to store it in the scope slot, too. If there is one.
    this->EmitScopeSlotStoreThis(funcInfo, nonLambdaFunc->thisScopeSlot);
}

void ByteCodeGenerator::EmitClassConstructorEndCode(FuncInfo *funcInfo)
{
    if (funcInfo->thisPointerRegister != Js::Constants::NoRegister)
    {
        // We need to try and load 'this' from the scope slot, if there is one.
        EmitScopeSlotLoadThis(funcInfo, funcInfo->thisPointerRegister);
        this->Writer()->Reg2(Js::OpCode::Ld_A, ByteCodeGenerator::ReturnRegister, funcInfo->thisPointerRegister);
    }
    else
    {
        // This is the case where we don't have any references to this or super in the constructor or any nested lambda functions.
        // We know 'this' must be undecl so let's just emit the ReferenceError as part of the fallthrough.
        EmitUseBeforeDeclarationRuntimeError(this, ByteCodeGenerator::ReturnRegister, true);
    }
}

void ByteCodeGenerator::EmitBaseClassConstructorThisObject(FuncInfo *funcInfo)
{
    this->Writer()->Reg2(Js::OpCode::NewScObjectNoCtorFull, funcInfo->thisPointerRegister, funcInfo->newTargetRegister);
}

void ByteCodeGenerator::EmitInternalScopedSlotLoad(FuncInfo *funcInfo, Js::RegSlot slot, Js::RegSlot symbolRegister, bool chkUndecl)
{
    Scope* scope = nullptr;

    if (funcInfo->IsLambda())
    {
        Js::PropertyId envIndex = -1;
        GetEnclosingNonLambdaScope(funcInfo, scope, envIndex);

        EmitInternalScopedSlotLoad(funcInfo, scope, envIndex, slot, symbolRegister, chkUndecl);
    }
    else
    {
        scope = funcInfo->GetBodyScope();

        EmitInternalScopedSlotLoad(funcInfo, scope, -1, slot, symbolRegister, chkUndecl);
    }
}

void ByteCodeGenerator::EmitInternalScopedSlotLoad(FuncInfo *funcInfo, Scope *scope, Js::PropertyId envIndex, Js::RegSlot slot, Js::RegSlot symbolRegister, bool chkUndecl)
{
    Assert(slot != Js::Constants::NoProperty);
    Js::ProfileId profileId = funcInfo->FindOrAddSlotProfileId(scope, symbolRegister);
    Js::OpCode opcode;

    Js::RegSlot scopeLocation = scope->GetLocation();
    opcode = this->GetLdSlotOp(scope, envIndex, scopeLocation, funcInfo);
    slot += (scope->GetIsObject() ? 0 : Js::ScopeSlots::FirstSlotIndex);

    if (envIndex != -1)
    {
        this->m_writer.SlotI2(opcode, symbolRegister, envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var), slot, profileId);
    }
    else if (scopeLocation != Js::Constants::NoRegister &&
             (scopeLocation == funcInfo->frameSlotsRegister || scopeLocation == funcInfo->frameObjRegister))
    {
        this->m_writer.SlotI1(opcode, symbolRegister, slot, profileId);
    }
    else
    {
        this->m_writer.Slot(opcode, symbolRegister, scopeLocation, slot, profileId);
    }

    if (chkUndecl)
    {
        this->m_writer.Reg1(Js::OpCode::ChkUndecl, symbolRegister);
    }
}

void ByteCodeGenerator::EmitInternalScopedSlotStore(FuncInfo *funcInfo, Js::RegSlot slot, Js::RegSlot symbolRegister)
{
    Assert(slot != Js::Constants::NoProperty);

    Scope* scope = nullptr;
    Js::OpCode opcode;

    Js::PropertyId envIndex = -1;
    if (funcInfo->IsLambda())
    {
        GetEnclosingNonLambdaScope(funcInfo, scope, envIndex);
    }
    else
    {
        scope = funcInfo->GetBodyScope();
    }

    Js::RegSlot scopeLocation = scope->GetLocation();
    opcode = this->GetStSlotOp(scope, envIndex, scopeLocation, false, funcInfo);
    slot += (scope->GetIsObject() ? 0 : Js::ScopeSlots::FirstSlotIndex);

    if (envIndex != -1)
    {
        this->m_writer.SlotI2(opcode, symbolRegister, envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var), slot);
    }
    else if (scopeLocation != Js::Constants::NoRegister &&
             (scopeLocation == funcInfo->frameSlotsRegister || scopeLocation == funcInfo->frameObjRegister))
    {
        this->m_writer.SlotI1(opcode, symbolRegister, slot);
    }
    else if (scope->GetIsObject())
    {
        this->m_writer.Slot(opcode, symbolRegister, scopeLocation, slot);
    }
    else
    {
        this->m_writer.SlotI2(opcode, symbolRegister, scope->GetInnerScopeIndex(), slot);
    }
}

void ByteCodeGenerator::EmitInternalScopeObjInit(FuncInfo *funcInfo, Scope *scope, Js::RegSlot valueLocation, Js::PropertyId propertyId)
{
    Js::RegSlot scopeLocation = scope->GetLocation();
    Js::OpCode opcode = this->GetInitFldOp(scope, scopeLocation, funcInfo);
    if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
    {
        uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, true);
        this->m_writer.ElementP(opcode, valueLocation, cacheId);
    }
    else if (scope->HasInnerScopeIndex())
    {
        uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->InnerScopeToRegSlot(scope), propertyId, false, true);
        this->m_writer.ElementPIndexed(opcode, valueLocation, scope->GetInnerScopeIndex(), cacheId);
    }
    else
    {
        uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, true);
        this->m_writer.PatchableProperty(opcode, valueLocation, scopeLocation, cacheId);
    }   
}

void ByteCodeGenerator::GetEnclosingNonLambdaScope(FuncInfo *funcInfo, Scope * &scope, Js::PropertyId &envIndex)
{
    envIndex = -1;
    for (scope = funcInfo->GetBodyScope()->GetEnclosingScope(); scope; scope = scope->GetEnclosingScope())
    {
        if (scope->GetMustInstantiate())
        {
            envIndex++;
        }
        if ((scope == scope->GetFunc()->GetBodyScope() && !scope->GetFunc()->IsLambda()) || scope->IsGlobalEvalBlockScope())
        {
            break;
        }
    }
}

void ByteCodeGenerator::EmitThis(FuncInfo *funcInfo, Js::RegSlot fromRegister)
{
    if (funcInfo->IsLambda())
    {
        Scope *scope;
        Js::PropertyId envIndex = -1;
        GetEnclosingNonLambdaScope(funcInfo, scope, envIndex);
        FuncInfo* parent = scope->GetFunc();

        if (parent->IsGlobalFunction())
        {
            if (this->flags & fscrEval)
            {
                scope = parent->GetGlobalEvalBlockScope();
                Js::PropertyId slot = parent->thisScopeSlot;
                EmitInternalScopedSlotLoad(funcInfo, scope, envIndex, slot, funcInfo->thisPointerRegister, false);
            }
            else
            {
                // Always load global object via LdThis of null to get the possibly protected via secureHostObject global object.
                this->m_writer.Reg2Int1(Js::OpCode::LdThis, funcInfo->thisPointerRegister, funcInfo->nullConstantRegister, this->GetModuleID());
            }
        }
        else if (!parent->IsClassConstructor() || parent->IsBaseClassConstructor())
        {
            // In a lambda inside a derived class constructor, 'this' should be loaded from the scope slot whenever 'this' is accessed.
            // It's safe to load 'this' into the register for base class constructors because there is no complex assignment to 'this'
            // via super call chain.
            Js::PropertyId slot = parent->thisScopeSlot;
            EmitInternalScopedSlotLoad(funcInfo, scope, envIndex, slot, funcInfo->thisPointerRegister, false);
        }
    }
    else if (funcInfo->byteCodeFunction->GetIsStrictMode() && (!funcInfo->IsGlobalFunction() || this->flags & fscrEval))
    {
        m_writer.Reg2(Js::OpCode::StrictLdThis, funcInfo->thisPointerRegister, fromRegister);
    }
    else
    {
        m_writer.Reg2Int1(Js::OpCode::LdThis, funcInfo->thisPointerRegister, fromRegister, this->GetModuleID());
    }
}

void ByteCodeGenerator::EmitLoadFormalIntoRegister(ParseNode *pnodeFormal, Js::RegSlot pos, FuncInfo *funcInfo)
{
    if (pnodeFormal->IsVarLetOrConst())
    {
        // Get the param from its argument position into its assigned register.
        // The position should match the location, otherwise, it has been shadowed by parameter with the same name
        Symbol *formal = pnodeFormal->sxVar.sym;
        if (formal->GetLocation() + 1 == pos)
        {
            // Transfer to the frame object, etc., if necessary.
            this->EmitLocalPropInit(formal->GetLocation(), formal, funcInfo);
        }

        if (ShouldTrackDebuggerMetadata() && !formal->IsInSlot(funcInfo))
        {
            Assert(!formal->GetHasInit());
            funcInfo->GetParsedFunctionBody()->InsertSymbolToRegSlotList(formal->GetName(), formal->GetLocation(), funcInfo->varRegsCount);
        }
    }
}

void ByteCodeGenerator::HomeArguments(FuncInfo *funcInfo)
{
    // Transfer formal parameters to their home locations on the local frame.
    if (funcInfo->GetHasArguments())
    {
        if (funcInfo->root->sxFnc.pnodeRest != nullptr)
        {
            // Since we don't have to iterate over arguments here, we'll trust the location to be correct.
            EmitLoadFormalIntoRegister(funcInfo->root->sxFnc.pnodeRest, funcInfo->root->sxFnc.pnodeRest->sxVar.sym->GetLocation() + 1, funcInfo);
        }

        // The arguments object creation helper does this work for us.
        return;
    }

    Js::ArgSlot pos = 1;
    auto loadFormal = [&](ParseNode *pnodeFormal)
    {
        EmitLoadFormalIntoRegister(pnodeFormal, pos, funcInfo);
        pos++;
    };
    MapFormals(funcInfo->root, loadFormal);
}

void ByteCodeGenerator::DefineLabels(FuncInfo *funcInfo)
{
    funcInfo->singleExit = m_writer.DefineLabel();
    SList<ParseNode *>::Iterator iter(&funcInfo->targetStatements);
    while (iter.Next())
    {
        ParseNode * node = iter.Data();
        node->sxStmt.breakLabel = m_writer.DefineLabel();
        node->sxStmt.continueLabel = m_writer.DefineLabel();
        node->emitLabels = true;
    }
}

void ByteCodeGenerator::EmitGlobalBody(FuncInfo *funcInfo)
{
    // Emit global code (global scope or eval), fixing up the return register with the implicit
    // return value.
    ParseNode *pnode = funcInfo->root->sxFnc.pnodeBody;
    ParseNode *pnodeLastVal = funcInfo->root->sxProg.pnodeLastValStmt;
    if (pnodeLastVal == nullptr)
    {
        // We're not guaranteed to compute any values, so fix up the return register at the top
        // in case.
        this->m_writer.Reg1(Js::OpCode::LdUndef, ReturnRegister);
    }

    while (pnode->nop == knopList)
    {
        ParseNode *stmt = pnode->sxBin.pnode1;
        if (stmt == pnodeLastVal)
        {
            pnodeLastVal = nullptr;
        }
        if (pnodeLastVal == nullptr && (this->flags & fscrReturnExpression))
        {
            EmitTopLevelStatement(stmt, funcInfo, true);
        }
        else
        {
            // Haven't hit the post-dominating return value yet,
            // so don't bother with the return register.
            EmitTopLevelStatement(stmt, funcInfo, false);
        }
        pnode = pnode->sxBin.pnode2;
    }
    EmitTopLevelStatement(pnode, funcInfo, false);
}

void ByteCodeGenerator::EmitFunctionBody(FuncInfo *funcInfo)
{
    // Emit a function body. Only explicit returns and the implicit "undef" at the bottom
    // get copied to the return register.
    ParseNode *pnodeBody = funcInfo->root->sxFnc.pnodeBody;
    ParseNode *pnode = pnodeBody;
    while (pnode->nop == knopList)
    {
        ParseNode *stmt = pnode->sxBin.pnode1;
        if (stmt->CapturesSyms())
        {
            CapturedSymMap *map = funcInfo->EnsureCapturedSymMap();
            SList<Symbol*> *list = map->Item(stmt);
            FOREACH_SLIST_ENTRY(Symbol*, sym, list)
            {
                if (!sym->GetIsCommittedToSlot())
                {
                    Assert(sym->GetLocation() != Js::Constants::NoProperty);
                    sym->SetIsCommittedToSlot();
                    ParseNode *decl = sym->GetDecl();
                    Assert(decl);
                    if (PHASE_TRACE(Js::DelayCapturePhase, funcInfo->byteCodeFunction))
                    {
                        Output::Print(L"--- DelayCapture: Committed symbol '%s' to slot.\n", sym->GetName());
                        Output::Flush();
                    }
                    this->EmitPropStore(sym->GetLocation(), sym, sym->GetPid(), funcInfo, decl->nop == knopLetDecl, decl->nop == knopConstDecl);
                }
            }
            NEXT_SLIST_ENTRY;
        }
        EmitTopLevelStatement(stmt, funcInfo, false);
        pnode = pnode->sxBin.pnode2;
    }
    Assert(!pnode->CapturesSyms());
    EmitTopLevelStatement(pnode, funcInfo, false);
}

void ByteCodeGenerator::EmitProgram(ParseNode *pnodeProg)
{
    // Indicate that the binding phase is over.
    this->isBinding = false;
    this->trackEnvDepth = true;
    AssignPropertyIds(pnodeProg->sxFnc.funcInfo->byteCodeFunction);

    long initSize = this->maxAstSize / AstBytecodeRatioEstimate;

    // Use the temp allocator in bytecode write temp buffer.
    m_writer.InitData(this->alloc, initSize);

#ifdef LOG_BYTECODE_AST_RATIO
    // log the max Ast size
    Output::Print(L"Max Ast size: %d", initSize);
#endif

    Assert(pnodeProg && pnodeProg->nop == knopProg);
    if (this->parentScopeInfo)
    {
        // Scope stack is already set up the way we want it, so don't visit the global scope.
        // Start emitting with the nested scope (i.e., the deferred function).
        this->EmitScopeList(pnodeProg->sxProg.pnodeScopes);
    }
    else
    {
        this->EmitScopeList(pnodeProg);
    }
}

void ByteCodeGenerator::EmitInitCapturedThis(FuncInfo* funcInfo, Scope* scope)
{
    if (scope->GetIsObject())
    {
        // Ensure space for the this slot
        this->EmitInternalScopeObjInit(funcInfo, scope, funcInfo->thisPointerRegister, Js::PropertyIds::_lexicalThisSlotSymbol);
    }
    else
    {
        this->EmitInternalScopedSlotStore(funcInfo, funcInfo->thisScopeSlot, funcInfo->thisPointerRegister);
    }
}

void ByteCodeGenerator::EmitInitCapturedNewTarget(FuncInfo* funcInfo, Scope* scope)
{
    if (scope->GetIsObject())
    {
        // Ensure space for the new.target slot
        this->EmitInternalScopeObjInit(funcInfo, scope, funcInfo->newTargetRegister, Js::PropertyIds::_lexicalNewTargetSymbol);
    }
    else
    {
        this->EmitInternalScopedSlotStore(funcInfo, funcInfo->newTargetScopeSlot, funcInfo->newTargetRegister);
    }
}

void EmitDestructuredObject(ParseNode *lhs, Js::RegSlot rhsLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);
void EmitDestructuredValueOrInitializer(ParseNodePtr lhsElementNode, Js::RegSlot rhsLocation, ParseNodePtr initializer, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);

void ByteCodeGenerator::EmitDefaultArgs(FuncInfo *funcInfo, ParseNode *pnode)
{
    auto emitDefaultArg = [&](ParseNode *pnodeArg)
    {
        if (pnodeArg->nop == knopParamPattern)
        {
            this->StartStatement(pnodeArg);

            Assert(pnodeArg->sxParamPattern.location != Js::Constants::NoRegister);
            ParseNodePtr pnode1 = pnodeArg->sxParamPattern.pnode1;

            if (pnode1->IsPattern())
            {
                EmitAssignment(nullptr, pnode1, pnodeArg->sxParamPattern.location, this, funcInfo);
            }
            else
            {
                Assert(pnode1->nop == knopAsg);
                Assert(pnode1->sxBin.pnode1->IsPattern());
                EmitDestructuredValueOrInitializer(pnode1->sxBin.pnode1, pnodeArg->sxParamPattern.location, pnode1->sxBin.pnode2, this, funcInfo);
            }
            this->EndStatement(pnodeArg);
            return;
        }
        else if (pnodeArg->IsVarLetOrConst())
        {
            Js::RegSlot location = pnodeArg->sxVar.sym->GetLocation();

            if (pnodeArg->sxVar.pnodeInit == nullptr)
            {
                // Since the formal hasn't been initialized in LdLetHeapArguments, we'll initialize it here.
                EmitPropStore(location, pnodeArg->sxVar.sym, pnodeArg->sxVar.pid, funcInfo, true);

                pnodeArg->sxVar.sym->SetNeedDeclaration(false);
                return;
            }

            // Load the default argument if we got undefined, skip RHS evaluation otherwise.
            Js::ByteCodeLabel noDefaultLabel = this->m_writer.DefineLabel();
            Js::ByteCodeLabel endLabel = this->m_writer.DefineLabel();
            this->StartStatement(pnodeArg);
            m_writer.BrReg2(Js::OpCode::BrNeq_A, noDefaultLabel, location, funcInfo->undefinedConstantRegister);

            Emit(pnodeArg->sxVar.pnodeInit, this, funcInfo, false);
            pnodeArg->sxVar.sym->SetNeedDeclaration(false); // After emit to prevent foo(a = a)

            if (funcInfo->GetHasArguments() && pnodeArg->sxVar.sym->IsInSlot(funcInfo))
            {
                EmitPropStore(pnodeArg->sxVar.pnodeInit->location, pnodeArg->sxVar.sym, pnodeArg->sxVar.pid, funcInfo, true);

                m_writer.Br(endLabel);
            }
            else
            {
                EmitAssignment(nullptr, pnodeArg, pnodeArg->sxVar.pnodeInit->location, this, funcInfo);
            }

            funcInfo->ReleaseLoc(pnodeArg->sxVar.pnodeInit);

            m_writer.MarkLabel(noDefaultLabel);

            if (funcInfo->GetHasArguments() && pnodeArg->sxVar.sym->IsInSlot(funcInfo))
            {
                EmitPropStore(location, pnodeArg->sxVar.sym, pnodeArg->sxVar.pid, funcInfo, true);

                m_writer.MarkLabel(endLabel);
            }

            this->EndStatement(pnodeArg);
        }
    };

    // If the function is async, we wrap the default arguments in a try catch and reject a Promise in case of error.
    if (pnode->sxFnc.IsAsync())
    {
        uint cacheId;
        Js::ByteCodeLabel catchLabel = m_writer.DefineLabel();
        Js::ByteCodeLabel doneLabel = m_writer.DefineLabel();
        Js::RegSlot catchArgLocation = funcInfo->AcquireTmpRegister();
        Js::RegSlot promiseLocation = funcInfo->AcquireTmpRegister();
        Js::RegSlot rejectLocation = funcInfo->AcquireTmpRegister();

        // try
        m_writer.RecordCrossFrameEntryExitRecord(/* isEnterBlock = */ true);
        m_writer.Br(Js::OpCode::TryCatch, catchLabel);

        // Rest cannot have a default argument, so we ignore it.
        MapFormalsWithoutRest(pnode, emitDefaultArg);

        m_writer.RecordCrossFrameEntryExitRecord(/* isEnterBlock = */ false);
        m_writer.Empty(Js::OpCode::Leave);
        m_writer.Br(doneLabel);

        // catch
        m_writer.MarkLabel(catchLabel);
        m_writer.Reg1(Js::OpCode::Catch, catchArgLocation);

        m_writer.RecordCrossFrameEntryExitRecord(/* isEnterBlock = */ true);
        m_writer.Empty(Js::OpCode::Nop);

        // return Promise.reject(error);
        cacheId = funcInfo->FindOrAddRootObjectInlineCacheId(Js::PropertyIds::Promise, false, false);
        m_writer.PatchableRootProperty(Js::OpCode::LdRootFld, promiseLocation, cacheId, false, false);

        EmitInvoke(rejectLocation, promiseLocation, Js::PropertyIds::reject, this, funcInfo, catchArgLocation);

        m_writer.Reg2(Js::OpCode::Ld_A, ByteCodeGenerator::ReturnRegister, rejectLocation);

        m_writer.RecordCrossFrameEntryExitRecord(/* isEnterBlock = */ false);
        m_writer.Empty(Js::OpCode::Leave);
        m_writer.Br(funcInfo->singleExit);
        m_writer.Empty(Js::OpCode::Leave);

        m_writer.MarkLabel(doneLabel);

        this->SetHasTry(true);

        funcInfo->ReleaseTmpRegister(rejectLocation);
        funcInfo->ReleaseTmpRegister(promiseLocation);
        funcInfo->ReleaseTmpRegister(catchArgLocation);
    }
    else
    {
        // Rest cannot have a default argument, so we ignore it.
        MapFormalsWithoutRest(pnode, emitDefaultArg);
    }
}

void ByteCodeGenerator::EmitOneFunction(ParseNode *pnode)
{
    Assert(pnode && (pnode->nop == knopProg || pnode->nop == knopFncDecl));
    FuncInfo *funcInfo = pnode->sxFnc.funcInfo;
    Assert(funcInfo != nullptr);

    if (funcInfo->IsFakeGlobalFunction(this->flags))
    {
        return;
    }

    Js::ParseableFunctionInfo* deferParseFunction = funcInfo->byteCodeFunction;
    deferParseFunction->SetGrfscr(deferParseFunction->GetGrfscr() | (this->flags & ~fscrDeferredFncExpression));
    deferParseFunction->SetSourceInfo(this->GetCurrentSourceIndex(),
        funcInfo->root,
        !!(this->flags & fscrEvalCode),
        ((this->flags & fscrDynamicCode) && !(this->flags & fscrEvalCode)));

    deferParseFunction->SetInParamsCount(funcInfo->inArgsCount);
    if (pnode->sxFnc.HasDefaultArguments())
    {
        deferParseFunction->SetReportedInParamsCount(pnode->sxFnc.firstDefaultArg + 1);
    }
    else
    {
        deferParseFunction->SetReportedInParamsCount(funcInfo->inArgsCount);
    }

    if (funcInfo->root->sxFnc.pnodeBody == nullptr)
    {
        if (scriptContext->GetConfig()->BindDeferredPidRefs() &&
            !PHASE_OFF1(Js::SkipNestedDeferredPhase))
        {
            deferParseFunction->BuildDeferredStubs(funcInfo->root);
        }
        return;
    }

    Js::FunctionBody* byteCodeFunction = funcInfo->GetParsedFunctionBody();
    // We've now done a full parse of this function, so we no longer need to remember the extents
    // and attributes of the top-level nested functions. (The above code has run for all of those,
    // so they have pointers to the stub sub-trees they need.)
    byteCodeFunction->SetDeferredStubs(nullptr);

    try
    {
        // Bug : 301517
        // In the debug mode the hasOnlyThis optimization needs to be disabled, since user can break in this function
        // and do operation on 'this' and its property, which may not be defined yet.
        if (funcInfo->root->sxFnc.HasOnlyThisStmts() && !IsInDebugMode())
        {
            byteCodeFunction->SetHasOnlyThisStmts(true);
        }

        if (byteCodeFunction->IsInlineApplyDisabled() || this->scriptContext->GetConfig()->IsNoNative())
        {
            if ((pnode->nop == knopFncDecl) && (funcInfo->GetHasHeapArguments()) && (!funcInfo->GetCallsEval()) && ApplyEnclosesArgs(pnode, this))
            {
                bool applyEnclosesArgs = true;
                for (ParseNode* pnodeVar = funcInfo->root->sxFnc.pnodeVars; pnodeVar; pnodeVar = pnodeVar->sxVar.pnodeNext)
                {
                    Symbol* sym = pnodeVar->sxVar.sym;
                    if (sym->GetSymbolType() == STVariable && !sym->GetIsArguments())
                    {
                        applyEnclosesArgs = false;
                        break;
                    }
                }
                auto constAndLetCheck = [](ParseNode *pnodeBlock, bool *applyEnclosesArgs)
                {
                    if (*applyEnclosesArgs)
                    {
                        for (auto lexvar = pnodeBlock->sxBlock.pnodeLexVars; lexvar; lexvar = lexvar->sxVar.pnodeNext)
                        {
                            Symbol* sym = lexvar->sxVar.sym;
                            if (sym->GetSymbolType() == STVariable && !sym->GetIsArguments())
                            {
                                *applyEnclosesArgs = false;
                                break;
                            }
                        }
                    }
                };
                constAndLetCheck(funcInfo->root->sxFnc.pnodeScopes, &applyEnclosesArgs);
                constAndLetCheck(funcInfo->root->sxFnc.pnodeBodyScope, &applyEnclosesArgs);
                funcInfo->SetApplyEnclosesArgs(applyEnclosesArgs);
            }
        }

        if (!funcInfo->IsGlobalFunction())
        {
            if (CanStackNestedFunc(funcInfo, true))
            {
#if DBG
                byteCodeFunction->SetCanDoStackNestedFunc();
#endif
                if (funcInfo->root->sxFnc.astSize <= PnFnc::MaxStackClosureAST)
                {
                    byteCodeFunction->SetStackNestedFunc(true);
                }
            }
        }

        InitScopeSlotArray(funcInfo);
        FinalizeRegisters(funcInfo, byteCodeFunction);
        DebugOnly(Js::RegSlot firstTmpReg = funcInfo->varRegsCount);

        // Reserve temp registers for the inner scopes. We prefer temps because the JIT will then renumber them
        // and see different lifetimes. (Note that debug mode requires permanent registers. See FinalizeRegisters.)
        uint innerScopeCount = funcInfo->InnerScopeCount();
        if (!this->IsInDebugMode())
        {
            byteCodeFunction->SetInnerScopeCount(innerScopeCount);
            if (innerScopeCount)
            {
                funcInfo->SetFirstInnerScopeReg(funcInfo->AcquireTmpRegister());
                for (uint i = 1; i < innerScopeCount; i++)
                {
                    funcInfo->AcquireTmpRegister();
                }
            }
        }

        funcInfo->inlineCacheMap = Anew(alloc, FuncInfo::InlineCacheMap,
            alloc,
            funcInfo->RegCount() // Pass the actual register count. // TODO: Check if we can reduce this count
            );
        funcInfo->rootObjectLoadInlineCacheMap = Anew(alloc, FuncInfo::RootObjectInlineCacheIdMap,
            alloc,
            10);
        funcInfo->rootObjectLoadMethodInlineCacheMap = Anew(alloc, FuncInfo::RootObjectInlineCacheIdMap,
            alloc,
            10);
        funcInfo->rootObjectStoreInlineCacheMap = Anew(alloc, FuncInfo::RootObjectInlineCacheIdMap,
            alloc,
            10);
        funcInfo->referencedPropertyIdToMapIndex = Anew(alloc, FuncInfo::RootObjectInlineCacheIdMap,
            alloc,
            10);

        byteCodeFunction->AllocateLiteralRegexArray();
        m_callSiteId = 0;
        m_writer.Begin(this, byteCodeFunction, alloc, this->DoJitLoopBodies(funcInfo), funcInfo->hasLoop);
        this->PushFuncInfo(L"EmitOneFunction", funcInfo);

        this->inPrologue = true;

        // Class constructors do not have a [[call]] slot but we don't implement a generic way to express this.
        // What we do is emit a check for the new flag here. If we don't have CallFlags_New set, the opcode will throw.
        // We need to do this before emitting 'this' since the base class constructor will try to construct a new object.
        if (funcInfo->IsClassConstructor())
        {
            m_writer.Empty(Js::OpCode::ChkNewCallFlag);
        }

        // For now, emit all constant loads at top of function (should instead put in closest dominator of uses).
        LoadAllConstants(funcInfo);
        HomeArguments(funcInfo);

        if (funcInfo->root->sxFnc.pnodeRest != nullptr)
        {
            byteCodeFunction->SetHasRestParameter();
        }

        if (funcInfo->thisScopeSlot != Js::Constants::NoRegister && !(funcInfo->IsLambda() || (funcInfo->IsGlobalFunction() && this->flags & fscrEval)))
        {
            EmitInitCapturedThis(funcInfo, funcInfo->bodyScope);
        }

        // Any function with a super reference or an eval call inside a class method needs to load super,
        if ((funcInfo->HasSuperReference() || (funcInfo->GetCallsEval() && funcInfo->root->sxFnc.IsClassMember()))
            // unless we are already inside the 'global' scope inside an eval (in which case 'ScopedLdSuper' is emitted at every 'super' reference).
            && !((GetFlags() & fscrEval) && funcInfo->IsGlobalFunction()))
        {
            if (funcInfo->IsLambda())
            {
                Scope *scope;
                Js::PropertyId envIndex = -1;
                GetEnclosingNonLambdaScope(funcInfo, scope, envIndex);

                FuncInfo* parent = scope->GetFunc();

                if (!parent->IsGlobalFunction())
                {
                    // lambda in non-global scope (eval and non-eval)
                    EmitInternalScopedSlotLoad(funcInfo, scope, envIndex, parent->superScopeSlot, funcInfo->superRegister);
                    if (funcInfo->superCtorRegister != Js::Constants::NoRegister)
                    {
                        EmitInternalScopedSlotLoad(funcInfo, scope, envIndex, parent->superCtorScopeSlot, funcInfo->superCtorRegister);
                    }
                }
                else if (!(GetFlags() & fscrEval))
                {
                    // lambda in non-eval global scope
                    m_writer.Reg1(Js::OpCode::LdUndef, funcInfo->superRegister);
                }
                // lambda in eval global scope: ScopedLdSuper will handle error throwing
            }
            else
            {
                m_writer.Reg1(Js::OpCode::LdSuper, funcInfo->superRegister);

                if (funcInfo->superCtorRegister != Js::Constants::NoRegister) // super() is allowed only in derived class constructors
                {
                    m_writer.Reg1(Js::OpCode::LdSuperCtor, funcInfo->superCtorRegister);
                }

                if (!funcInfo->IsGlobalFunction())
                {
                    if (funcInfo->bodyScope->GetIsObject() && funcInfo->bodyScope->GetLocation() != Js::Constants::NoRegister)
                    {
                        // Stash the super reference in case something inside the eval or lambda references it.
                        uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->bodyScope->GetLocation(), Js::PropertyIds::_superReferenceSymbol, false, true);
                        m_writer.ElementP(Js::OpCode::InitLocalFld, funcInfo->superRegister, cacheId);
                        if (funcInfo->superCtorRegister != Js::Constants::NoRegister)
                        {
                            cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->bodyScope->GetLocation(), Js::PropertyIds::_superCtorReferenceSymbol, false, true);
                            m_writer.ElementP(Js::OpCode::InitLocalFld, funcInfo->superCtorRegister, cacheId);
                        }
                    }
                    else if (funcInfo->superScopeSlot == Js::Constants::NoProperty || funcInfo->superCtorScopeSlot == Js::Constants::NoProperty)
                    {
                        // While the diag locals walker will pick up super from scoped slots or an activation object,
                        // it will not pick it up when it is only in a register.
                        byteCodeFunction->InsertSymbolToRegSlotList(funcInfo->superRegister, Js::PropertyIds::_superReferenceSymbol, funcInfo->varRegsCount);
                        if (funcInfo->superCtorRegister != Js::Constants::NoRegister)
                        {
                            byteCodeFunction->InsertSymbolToRegSlotList(funcInfo->superCtorRegister, Js::PropertyIds::_superCtorReferenceSymbol, funcInfo->varRegsCount);
                        }
                    }
                }
            }
        }

        if (funcInfo->newTargetScopeSlot != Js::Constants::NoRegister && !funcInfo->IsGlobalFunction())
        {
            EmitInitCapturedNewTarget(funcInfo, funcInfo->bodyScope);
        }

        // We don't want to load super if we are already in an eval. ScopedLdSuper will take care of loading super in that case.
        if (!(GetFlags() & fscrEval) && !funcInfo->bodyScope->GetIsObject())
        {
            if (funcInfo->superScopeSlot != Js::Constants::NoRegister)
            {
                this->EmitInternalScopedSlotStore(funcInfo, funcInfo->superScopeSlot, funcInfo->superRegister);
            }

            if (funcInfo->superCtorScopeSlot != Js::Constants::NoRegister)
            {
                this->EmitInternalScopedSlotStore(funcInfo, funcInfo->superCtorScopeSlot, funcInfo->superCtorRegister);
            }
        }

        if (byteCodeFunction->DoStackNestedFunc())
        {
            uint nestedCount = byteCodeFunction->GetNestedCount();
            for (uint i = 0; i < nestedCount; i++)
            {
                Js::FunctionProxy * nested = byteCodeFunction->GetNestedFunc(i);
                if (nested->IsFunctionBody())
                {
                    nested->GetFunctionBody()->SetStackNestedFuncParent(byteCodeFunction);
                }
            }
        }

        if (scriptContext->GetConfig()->IsLetAndConstEnabled() && funcInfo->IsGlobalFunction())
        {
            EnsureNoRedeclarations(pnode->sxFnc.pnodeScopes, funcInfo);
        }

        // Emit all scope-wide function definitions before emitting function bodies
        // so that calls may reference functions they precede lexically.
        // Note, global eval scope is a fake local scope and is handled as if it were
        // a lexical block instead of a true global scope, so do not define the functions
        // here. They will be defined during BeginEmitBlock.
        if (!(funcInfo->IsGlobalFunction() && this->IsEvalWithBlockScopingNoParentScopeInfo()))
        {
            DefineFunctions(funcInfo);
        }
        DefineUserVars(funcInfo);

        ::BeginEmitBlock(pnode->sxFnc.pnodeScopes, this, funcInfo);

        if (pnode->sxFnc.pnodeBodyScope != nullptr)
        {
            ::BeginEmitBlock(pnode->sxFnc.pnodeBodyScope, this, funcInfo);
        }
        DefineLabels(funcInfo);

        if (!pnode->sxFnc.IsSimpleParameterList())
        {
            EmitDefaultArgs(funcInfo, pnode);
        }
        else if (funcInfo->GetHasArguments() && !NeedScopeObjectForArguments(funcInfo, pnode))
        {
            // If we didn't create a scope object and didn't have default args, we still need to transfer the formals to their slots.
            MapFormalsWithoutRest(pnode, [&](ParseNode *pnodeArg) { EmitPropStore(pnodeArg->sxVar.sym->GetLocation(), pnodeArg->sxVar.sym, pnodeArg->sxVar.pid, funcInfo); });
        }

        // Rest needs to trigger use before declaration until all default args have been processed.
        if (pnode->sxFnc.pnodeRest != nullptr)
        {
            pnode->sxFnc.pnodeRest->sxVar.sym->SetNeedDeclaration(false);
        }

        this->inPrologue = false;

        if (funcInfo->IsGlobalFunction())
        {
            EmitGlobalBody(funcInfo);
        }
        else
        {
            EmitFunctionBody(funcInfo);
        }

        if (pnode->sxFnc.pnodeBodyScope != nullptr)
        {
            ::EndEmitBlock(pnode->sxFnc.pnodeBodyScope, this, funcInfo);
        }
        ::EndEmitBlock(pnode->sxFnc.pnodeScopes, this, funcInfo);

        if (!this->IsInDebugMode())
        {
            // Release the temp registers that we reserved for inner scopes above.
            if (innerScopeCount)
            {
                Js::RegSlot tmpReg = funcInfo->FirstInnerScopeReg() + innerScopeCount - 1;
                for (uint i = 0; i < innerScopeCount; i++)
                {
                    funcInfo->ReleaseTmpRegister(tmpReg);
                    tmpReg--;
                }
            }
        }

        Assert(funcInfo->firstTmpReg == firstTmpReg);
        Assert(funcInfo->curTmpReg == firstTmpReg);
        Assert(byteCodeFunction->GetFirstTmpReg() == firstTmpReg + byteCodeFunction->GetConstantCount());

        byteCodeFunction->SetVarCount(funcInfo->varRegsCount);
        byteCodeFunction->SetOutParamDepth(funcInfo->outArgsMaxDepth);

        // Do a uint32 add just to verify that we haven't overflowed the reg slot type.
        UInt32Math::Add(funcInfo->varRegsCount, funcInfo->constRegsCount);

#if DBG_DUMP
        if (PHASE_STATS1(Js::ByteCodePhase))
        {
            Output::Print(L" BCode: %-10d, Aux: %-10d, AuxC: %-10d Total: %-10d,  %s\n",
                m_writer.ByteCodeDataSize(),
                m_writer.AuxiliaryDataSize(),
                m_writer.AuxiliaryContextDataSize(),
                m_writer.ByteCodeDataSize() + m_writer.AuxiliaryDataSize() + m_writer.AuxiliaryContextDataSize(),
                funcInfo->name);

            this->scriptContext->byteCodeDataSize += m_writer.ByteCodeDataSize();
            this->scriptContext->byteCodeAuxiliaryDataSize += m_writer.AuxiliaryDataSize();
            this->scriptContext->byteCodeAuxiliaryContextDataSize += m_writer.AuxiliaryContextDataSize();
        }
#endif

        this->MapCacheIdsToPropertyIds(funcInfo);
        this->MapReferencedPropertyIds(funcInfo);

        Assert(this->TopFuncInfo() == funcInfo);
        PopFuncInfo(L"EmitOneFunction");
        m_writer.SetCallSiteCount(m_callSiteId);
#ifdef LOG_BYTECODE_AST_RATIO
        m_writer.End(funcInfo->root->sxFnc.astSize, this->maxAstSize);
#else
        m_writer.End();
#endif
    }
    catch (...)
    {
        // Failed to generate byte-code for this function body (likely OOM or stack overflow). Notify the function body so that
        // it can revert intermediate state changes that may have taken place during byte code generation before the failure.
        byteCodeFunction->ResetByteCodeGenState();
        m_writer.Reset();
        throw;
    }

#ifdef PERF_HINT
    if (PHASE_TRACE1(Js::PerfHintPhase) && !byteCodeFunction->GetIsGlobalFunc())
    {
        if (byteCodeFunction->GetHasTry())
        {
            WritePerfHint(PerfHints::HasTryBlock_Verbose, byteCodeFunction);
        }

        if (funcInfo->GetCallsEval())
        {
            WritePerfHint(PerfHints::CallsEval_Verbose, byteCodeFunction);
        }
        else if (funcInfo->GetChildCallsEval())
        {
            WritePerfHint(PerfHints::ChildCallsEval, byteCodeFunction);
        }
    }
#endif


    byteCodeFunction->SetInitialDefaultEntryPoint();

    byteCodeFunction->SetIsByteCodeDebugMode(this->IsInDebugMode());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (byteCodeFunction->IsByteCodeDebugMode() != scriptContext->IsInDebugMode()) // debug mode mismatch
    {
        if (m_utf8SourceInfo->GetIsLibraryCode())
        {
            Assert(!byteCodeFunction->IsByteCodeDebugMode()); // Library script byteCode is never in debug mode
        }
        else
        {
            Js::Throw::FatalInternalError();
        }
    }
#endif

#if DBG_DUMP
    if (PHASE_DUMP(Js::ByteCodePhase, funcInfo->byteCodeFunction) && Js::Configuration::Global.flags.Verbose)
    {
        pnode->Dump();
    }
    if (this->Trace() || PHASE_DUMP(Js::ByteCodePhase, funcInfo->byteCodeFunction))
    {
        Js::ByteCodeDumper::Dump(byteCodeFunction);
    }
    if (PHASE_DUMP(Js::DebuggerScopePhase, funcInfo->byteCodeFunction))
    {
        byteCodeFunction->DumpScopes();
    }
#endif
#if ENABLE_NATIVE_CODEGEN
    if ((!PHASE_OFF(Js::BackEndPhase, funcInfo->byteCodeFunction))
        && !this->forceNoNative
        && !this->scriptContext->GetConfig()->IsNoNative())
    {
        GenerateFunction(this->scriptContext->GetNativeCodeGenerator(), byteCodeFunction);
    }
#endif
}

void ByteCodeGenerator::MapCacheIdsToPropertyIds(FuncInfo *funcInfo)
{
    Js::FunctionBody *functionBody = funcInfo->GetParsedFunctionBody();
    uint rootObjectLoadInlineCacheStart = funcInfo->GetInlineCacheCount();
    uint rootObjectLoadMethodInlineCacheStart = rootObjectLoadInlineCacheStart + funcInfo->GetRootObjectLoadInlineCacheCount();
    uint rootObjectStoreInlineCacheStart = rootObjectLoadMethodInlineCacheStart + funcInfo->GetRootObjectLoadMethodInlineCacheCount();
    uint totalFieldAccessInlineCacheCount = rootObjectStoreInlineCacheStart + funcInfo->GetRootObjectStoreInlineCacheCount();

    functionBody->CreateCacheIdToPropertyIdMap(rootObjectLoadInlineCacheStart, rootObjectLoadMethodInlineCacheStart,
        rootObjectStoreInlineCacheStart, totalFieldAccessInlineCacheCount, funcInfo->GetIsInstInlineCacheCount());

    if (totalFieldAccessInlineCacheCount == 0)
    {
        return;
    }

    funcInfo->inlineCacheMap->Map([functionBody](Js::RegSlot regSlot, FuncInfo::InlineCacheIdMap *inlineCacheIdMap)
    {
        inlineCacheIdMap->Map([functionBody](Js::PropertyId propertyId, FuncInfo::InlineCacheList* inlineCacheList)
        {
            if (inlineCacheList)
            {
                inlineCacheList->Iterate([functionBody, propertyId](InlineCacheUnit cacheUnit)
                {
                    CompileAssert(offsetof(InlineCacheUnit, cacheId) == offsetof(InlineCacheUnit, loadCacheId));
                    if (cacheUnit.loadCacheId != -1)
                    {
                        functionBody->SetPropertyIdForCacheId(cacheUnit.loadCacheId, propertyId);
                    }
                    if (cacheUnit.loadMethodCacheId != -1)
                    {
                        functionBody->SetPropertyIdForCacheId(cacheUnit.loadMethodCacheId, propertyId);
                    }
                    if (cacheUnit.storeCacheId != -1)
                    {
                        functionBody->SetPropertyIdForCacheId(cacheUnit.storeCacheId, propertyId);
                    }
                });
            }
        });
    });

    funcInfo->rootObjectLoadInlineCacheMap->Map([functionBody, rootObjectLoadInlineCacheStart](Js::PropertyId propertyId, uint cacheId)
    {
        functionBody->SetPropertyIdForCacheId(cacheId + rootObjectLoadInlineCacheStart, propertyId);
    });
    funcInfo->rootObjectLoadMethodInlineCacheMap->Map([functionBody, rootObjectLoadMethodInlineCacheStart](Js::PropertyId propertyId, uint cacheId)
    {
        functionBody->SetPropertyIdForCacheId(cacheId + rootObjectLoadMethodInlineCacheStart, propertyId);
    });
    funcInfo->rootObjectStoreInlineCacheMap->Map([functionBody, rootObjectStoreInlineCacheStart](Js::PropertyId propertyId, uint cacheId)
    {
        functionBody->SetPropertyIdForCacheId(cacheId + rootObjectStoreInlineCacheStart, propertyId);
    });

    SListBase<uint>::Iterator valueOfIter(&funcInfo->valueOfStoreCacheIds);
    while (valueOfIter.Next())
    {
        functionBody->SetPropertyIdForCacheId(valueOfIter.Data(), Js::PropertyIds::valueOf);
    }

    SListBase<uint>::Iterator toStringIter(&funcInfo->toStringStoreCacheIds);
    while (toStringIter.Next())
    {
        functionBody->SetPropertyIdForCacheId(toStringIter.Data(), Js::PropertyIds::toString);
    }

#if DBG
    functionBody->VerifyCacheIdToPropertyIdMap();
#endif
}

void ByteCodeGenerator::MapReferencedPropertyIds(FuncInfo * funcInfo)
{
    Js::FunctionBody *functionBody = funcInfo->GetParsedFunctionBody();
    uint referencedPropertyIdCount = funcInfo->GetReferencedPropertyIdCount();
    functionBody->CreateReferencedPropertyIdMap(referencedPropertyIdCount);

    funcInfo->referencedPropertyIdToMapIndex->Map([functionBody](Js::PropertyId propertyId, uint mapIndex)
    {
        functionBody->SetReferencedPropertyIdWithMapIndex(mapIndex, propertyId);
    });

#if DBG
    functionBody->VerifyReferencedPropertyIdMap();
#endif
}

void ByteCodeGenerator::EmitScopeList(ParseNode *pnode)
{
    while (pnode)
    {
        switch (pnode->nop)
        {
            case knopFncDecl:
#ifndef TEMP_DISABLE_ASMJS
            if (pnode->sxFnc.GetAsmjsMode())
            {
                Js::ExclusiveContext context(this, GetScriptContext());
                if (Js::AsmJSCompiler::Compile(&context, pnode, pnode->sxFnc.pnodeArgs))
                {
                    pnode = pnode->sxFnc.pnodeNext;
                    break;
                }
                else if (CONFIG_FLAG(AsmJsStopOnError))
                {
                    exit(JSERR_AsmJsCompileError);
                }
                else if (!(flags & fscrDeferFncParse))
                {
                    // If deferral is not allowed, throw and reparse everything with asm.js disabled.
                    throw Js::AsmJsParseException();
                }
            }
#endif
            // FALLTHROUGH
        case knopProg:
            if (pnode->sxFnc.funcInfo)
            {
                this->StartEmitFunction(pnode);
                this->EmitScopeList(pnode->sxFnc.pnodeScopes);
                this->EmitOneFunction(pnode);
                this->EndEmitFunction(pnode);
            }
            pnode = pnode->sxFnc.pnodeNext;
            break;

        case knopBlock:
            this->StartEmitBlock(pnode);
            this->EmitScopeList(pnode->sxBlock.pnodeScopes);
            this->EndEmitBlock(pnode);
            pnode = pnode->sxBlock.pnodeNext;
            break;

        case knopCatch:
            this->StartEmitCatch(pnode);
            this->EmitScopeList(pnode->sxCatch.pnodeScopes);
            this->EndEmitCatch(pnode);
            pnode = pnode->sxCatch.pnodeNext;
            break;

        case knopWith:
            this->StartEmitWith(pnode);
            this->EmitScopeList(pnode->sxWith.pnodeScopes);
            this->EndEmitWith(pnode);
            pnode = pnode->sxWith.pnodeNext;
            break;

        default:
            AssertMsg(false, "Unexpected opcode in tree of scopes");
            break;
        }
    }
}

void EnsureFncDeclScopeSlot(ParseNode *pnodeFnc, FuncInfo *funcInfo)
{
    if (pnodeFnc->sxFnc.pnodeName)
    {
        Assert(pnodeFnc->sxFnc.pnodeName->nop == knopVarDecl);
        Symbol *sym = pnodeFnc->sxFnc.pnodeName->sxVar.sym;
        if (sym)
        {
            sym->EnsureScopeSlot(funcInfo);
        }
    }
}

// Similar to EnsureFncScopeSlot visitor function, but verifies that a slot is needed before assigning it.
void CheckFncDeclScopeSlot(ParseNode *pnodeFnc, FuncInfo *funcInfo)
{
    if (pnodeFnc->sxFnc.pnodeName && pnodeFnc->sxFnc.pnodeName->nop == knopVarDecl)
    {
        Assert(pnodeFnc->sxFnc.pnodeName->nop == knopVarDecl);
        Symbol *sym = pnodeFnc->sxFnc.pnodeName->sxVar.sym;
        if (sym && sym->NeedsSlotAlloc(funcInfo))
        {
            sym->EnsureScopeSlot(funcInfo);
        }
    }
}

void ByteCodeGenerator::StartEmitFunction(ParseNode *pnodeFnc)
{
    Assert(pnodeFnc->nop == knopFncDecl || pnodeFnc->nop == knopProg);

    FuncInfo *funcInfo = pnodeFnc->sxFnc.funcInfo;

    if (funcInfo->byteCodeFunction->IsFunctionParsed() &&
        !(flags & (fscrEval | fscrImplicitThis | fscrImplicitParents)))
    {
        // Only set the environment depth if it's truly known (i.e., not in eval or event handler).
        funcInfo->GetParsedFunctionBody()->SetEnvDepth(this->envDepth);
    }

    if (funcInfo->GetCallsEval())
    {
        funcInfo->byteCodeFunction->SetDontInline(true);
    }

    Scope * const funcExprScope = funcInfo->funcExprScope;
    if (funcExprScope)
    {
        if (funcInfo->GetCallsEval())
        {
            Assert(funcExprScope->GetIsObject());
        }

        if (funcExprScope->GetIsObject())
        {
            funcExprScope->SetCapturesAll(true);
            funcExprScope->SetMustInstantiate(true);
            PushScope(funcExprScope);
        }
        else
        {
            Symbol *sym = funcInfo->root->sxFnc.GetFuncSymbol();
            funcInfo->bodyScope->AddSymbol(sym);
        }
    }

    Scope * const bodyScope = funcInfo->bodyScope;

    if (pnodeFnc->nop != knopProg)
    {
        if (!bodyScope->GetIsObject() && NeedObjectAsFunctionScope(funcInfo, pnodeFnc))
        {
            Assert(bodyScope->GetIsObject());
        }

        if (bodyScope->GetIsObject())
        {
            bodyScope->SetLocation(funcInfo->frameObjRegister);
        }
        else
        {
            bodyScope->SetLocation(funcInfo->frameSlotsRegister);
        }

        bodyScope->SetMustInstantiate(funcInfo->frameObjRegister != Js::Constants::NoRegister || funcInfo->frameSlotsRegister != Js::Constants::NoRegister);

        if (bodyScope->GetIsObject())
        {
            // Win8 908700: Disable under F12 debugger because there are too many cached scopes holding onto locals.
            funcInfo->SetHasCachedScope(
                !PHASE_OFF(Js::CachedScopePhase, funcInfo->byteCodeFunction) &&
                !funcInfo->Escapes() &&
                funcInfo->frameObjRegister != Js::Constants::NoRegister &&
                !ApplyEnclosesArgs(pnodeFnc, this) &&
                (PHASE_FORCE(Js::CachedScopePhase, funcInfo->byteCodeFunction) || !IsInDebugMode()));

            if (funcInfo->GetHasCachedScope())
            {
                Assert(funcInfo->funcObjRegister == Js::Constants::NoRegister);
                Symbol *funcSym = funcInfo->root->sxFnc.GetFuncSymbol();
                if (funcSym && funcSym->GetFuncExpr())
                {
                    if (funcSym->GetLocation() == Js::Constants::NoRegister)
                    {
                        funcInfo->funcObjRegister = funcInfo->NextVarRegister();
                    }
                    else
                    {
                        funcInfo->funcObjRegister = funcSym->GetLocation();
                    }
                }
                else
                {
                    funcInfo->funcObjRegister = funcInfo->NextVarRegister();
                }
                Assert(funcInfo->funcObjRegister != Js::Constants::NoRegister);
            }

            ParseNode *pnode;
            Symbol *sym;

            PushScope(bodyScope);

            // Turns on capturesAll temporarily if func has deferred child, so that the following EnsureScopeSlot
            // will allocate scope slots no matter if symbol hasNonLocalRefence or not.
            Js::ScopeInfo::AutoCapturesAllScope autoCapturesAllScope(bodyScope, funcInfo->HasDeferredChild());

            if (funcInfo->GetHasArguments())
            {
                // Process function's formal parameters
                MapFormals(pnodeFnc, [&](ParseNode *pnode)
                {
                    if (pnode->IsVarLetOrConst())
                    {
                        pnode->sxVar.sym->EnsureScopeSlot(funcInfo);
                    }
                });

                MapFormalsFromPattern(pnodeFnc, [&](ParseNode *pnode) { pnode->sxVar.sym->EnsureScopeSlot(funcInfo); });

                // Only allocate scope slot for "arguments" when really necessary. "hasDeferredChild"
                // doesn't require scope slot for "arguments" because inner functions can't access
                // outer function's arguments directly.
                sym = funcInfo->GetArgumentsSymbol();
                Assert(sym);
                if (sym->GetHasNonLocalReference() || autoCapturesAllScope.OldCapturesAll())
                {
                    sym->EnsureScopeSlot(funcInfo);
                }
            }

            sym = funcInfo->root->sxFnc.GetFuncSymbol();

            if (sym && sym->NeedsSlotAlloc(funcInfo))
            {
                if (funcInfo->funcExprScope && funcInfo->funcExprScope->GetIsObject())
                {
                    sym->SetScopeSlot(0);
                }
                else if (funcInfo->GetFuncExprNameReference())
                {
                    sym->EnsureScopeSlot(funcInfo);
                }
            }

            if (!funcInfo->GetHasArguments())
            {
                Symbol *formal;
                Js::ArgSlot pos = 1;
                auto moveArgToReg = [&](ParseNode *pnode)
                {
                    if (pnode->IsVarLetOrConst())
                    {
                        formal = pnode->sxVar.sym;
                        // Get the param from its argument position into its assigned register.
                        // The position should match the location; otherwise, it has been shadowed by parameter with the same name.
                        if (formal->GetLocation() + 1 == pos)
                        {
                            pnode->sxVar.sym->EnsureScopeSlot(funcInfo);
                        }
                    }
                    pos++;
                };
                MapFormals(pnodeFnc, moveArgToReg);
                MapFormalsFromPattern(pnodeFnc, [&](ParseNode *pnode) { pnode->sxVar.sym->EnsureScopeSlot(funcInfo); });
            }

            if (funcInfo->isThisLexicallyCaptured)
            {
                funcInfo->EnsureThisScopeSlot();
            }

            if (((!funcInfo->IsLambda() && funcInfo->GetCallsEval())
                || funcInfo->isSuperLexicallyCaptured))
            {
                if (funcInfo->superRegister != Js::Constants::NoRegister)
                {
                    funcInfo->EnsureSuperScopeSlot();
                }

                if (funcInfo->superCtorRegister != Js::Constants::NoRegister)
                {
                    funcInfo->EnsureSuperCtorScopeSlot();
                }
            }

            if (funcInfo->isNewTargetLexicallyCaptured)
            {
                funcInfo->EnsureNewTargetScopeSlot();
            }

            auto ensureFncDeclScopeSlots = [&](ParseNode *pnodeScope)
            {
                for (pnode = pnodeScope; pnode;)
                {
                    switch (pnode->nop)
                    {
                    case knopFncDecl:
                        if (pnode->sxFnc.IsDeclaration())
                        {
                            EnsureFncDeclScopeSlot(pnode, funcInfo);
                        }
                        pnode = pnode->sxFnc.pnodeNext;
                        break;
                    case knopBlock:
                        pnode = pnode->sxBlock.pnodeNext;
                        break;
                    case knopCatch:
                        pnode = pnode->sxCatch.pnodeNext;
                        break;
                    case knopWith:
                        pnode = pnode->sxWith.pnodeNext;
                        break;
                    }
                }
            };
            pnodeFnc->sxFnc.MapContainerScopes(ensureFncDeclScopeSlots);

            for (pnode = pnodeFnc->sxFnc.pnodeVars; pnode; pnode = pnode->sxVar.pnodeNext)
            {
                sym = pnode->sxVar.sym;
                if (!(pnode->sxVar.isBlockScopeFncDeclVar && sym->GetIsBlockVar()))
                {
                    if (sym->GetIsCatch() || (pnode->nop == knopVarDecl && sym->GetIsBlockVar()))
                    {
                        sym = funcInfo->bodyScope->FindLocalSymbol(sym->GetName());
                    }
                    if (sym->GetSymbolType() == STVariable && !sym->GetIsArguments())
                    {
                        sym->EnsureScopeSlot(funcInfo);
                    }
                }
            }

            if (pnodeFnc->sxFnc.pnodeBody)
            {
                Assert(pnodeFnc->sxFnc.pnodeScopes->nop == knopBlock);
                this->EnsureLetConstScopeSlots(pnodeFnc->sxFnc.pnodeBodyScope, funcInfo);
            }
        }
        else
        {
            ParseNode *pnode;
            Symbol *sym;

            // Don't rely on the Emit() pass to assign scope slots where needed, because peeps/shortcuts
            // may cause some expressions not to be emitted. Assign the slots we need before we start
            // emitting the prolog.
            // TODO: Investigate moving detection of non-local references to Emit() so we don't assign
            // slots to symbols that are never referenced in emitted code.

            if (funcInfo->isThisLexicallyCaptured)
            {
                funcInfo->EnsureThisScopeSlot();
            }

            if (funcInfo->isSuperLexicallyCaptured)
            {
                funcInfo->EnsureSuperScopeSlot();
            }

            if (funcInfo->isSuperCtorLexicallyCaptured)
            {
                funcInfo->EnsureSuperCtorScopeSlot();
            }

            if (funcInfo->isNewTargetLexicallyCaptured)
            {
                funcInfo->EnsureNewTargetScopeSlot();
            }

            pnodeFnc->sxFnc.MapContainerScopes([&](ParseNode *pnodeScope) { this->EnsureFncScopeSlots(pnodeScope, funcInfo); });

            PushScope(bodyScope);

            for (pnode = pnodeFnc->sxFnc.pnodeVars; pnode; pnode = pnode->sxVar.pnodeNext)
            {
                sym = pnode->sxVar.sym;
                if (!(pnode->sxVar.isBlockScopeFncDeclVar && sym->GetIsBlockVar()))
                {
                    if (sym->GetIsCatch() || (pnode->nop == knopVarDecl && sym->GetIsBlockVar()))
                    {
                        sym = funcInfo->bodyScope->FindLocalSymbol(sym->GetName());
                    }
                    if (sym->GetSymbolType() == STVariable && sym->NeedsSlotAlloc(funcInfo) && !sym->GetIsArguments())
                    {
                        sym->EnsureScopeSlot(funcInfo);
                    }
                }
            }

            auto ensureScopeSlot = [&](ParseNode *pnode)
            {
                if (pnode->IsVarLetOrConst())
                {
                    sym = pnode->sxVar.sym;
                    if (sym->GetSymbolType() == STFormal && sym->NeedsSlotAlloc(funcInfo))
                    {
                        sym->EnsureScopeSlot(funcInfo);
                    }
                }
            };
            // Process function's formal parameters
            MapFormals(pnodeFnc, ensureScopeSlot);
            MapFormalsFromPattern(pnodeFnc, ensureScopeSlot);

            if (pnodeFnc->sxFnc.pnodeBody)
            {
                this->EnsureLetConstScopeSlots(pnodeFnc->sxFnc.pnodeScopes, funcInfo);
                this->EnsureLetConstScopeSlots(pnodeFnc->sxFnc.pnodeBodyScope, funcInfo);
            }
        }
    }
    else
    {
        bool newScopeForEval = (funcInfo->byteCodeFunction->GetIsStrictMode() && (this->GetFlags() & fscrEval));

        if (newScopeForEval)
        {
            Assert(bodyScope->GetIsObject());
        }

        PushScope(bodyScope);
    }

    // Persist outer func scope info if nested func is deferred
    if (CONFIG_FLAG(DeferNested))
    {
        FuncInfo* parentFunc = TopFuncInfo();
        Js::ScopeInfo::SaveScopeInfoForDeferParse(this, parentFunc, funcInfo);
        PushFuncInfo(L"StartEmitFunction", funcInfo);
    }
}

void ByteCodeGenerator::EnsureLetConstScopeSlots(ParseNode *pnodeBlock, FuncInfo *funcInfo)
{
    bool hasNonLocalReference = pnodeBlock->sxBlock.GetCallsEval() || pnodeBlock->sxBlock.GetChildCallsEval();
    auto ensureLetConstSlots = ([this, pnodeBlock, funcInfo, hasNonLocalReference](ParseNode *pnode)
    {
        Symbol *sym = pnode->sxVar.sym;
        sym->EnsureScopeSlot(funcInfo);
        if (hasNonLocalReference)
        {
            sym->SetHasNonLocalReference(true, this);
        }
    });
    IterateBlockScopedVariables(pnodeBlock, ensureLetConstSlots);
}

void ByteCodeGenerator::EnsureFncScopeSlots(ParseNode *pnode, FuncInfo *funcInfo)
{
    while (pnode)
    {
        switch (pnode->nop)
        {
        case knopFncDecl:
            if (pnode->sxFnc.IsDeclaration())
            {
                CheckFncDeclScopeSlot(pnode, funcInfo);
            }
            pnode = pnode->sxFnc.pnodeNext;
            break;
        case knopBlock:
            pnode = pnode->sxBlock.pnodeNext;
            break;
        case knopCatch:
            pnode = pnode->sxCatch.pnodeNext;
            break;
        case knopWith:
            pnode = pnode->sxWith.pnodeNext;
            break;
        }
    }
}

void ByteCodeGenerator::EndEmitFunction(ParseNode *pnodeFnc)
{
    Assert(pnodeFnc->nop == knopFncDecl || pnodeFnc->nop == knopProg);
    Assert(pnodeFnc->nop == knopFncDecl && currentScope->GetEnclosingScope() != nullptr || pnodeFnc->nop == knopProg);

    PopScope(); // function body

    FuncInfo *funcInfo = pnodeFnc->sxFnc.funcInfo;

    Scope *scope = funcInfo->funcExprScope;
    if (scope && scope->GetMustInstantiate())
    {
        Assert(currentScope == scope);
        PopScope();
    }

    if (CONFIG_FLAG(DeferNested))
    {
        Assert(funcInfo == this->TopFuncInfo());
        PopFuncInfo(L"EndEmitFunction");
    }
}

void ByteCodeGenerator::StartEmitCatch(ParseNode *pnodeCatch)
{
    Assert(pnodeCatch->nop == knopCatch);

    Scope *scope = pnodeCatch->sxCatch.scope;
    FuncInfo *funcInfo = scope->GetFunc();

    // Catch scope is a dynamic object if it can be passed to a scoped lookup helper (i.e., eval is present or we're in an event handler).
    if (funcInfo->GetCallsEval() || funcInfo->GetChildCallsEval() || (this->flags & (fscrEval | fscrImplicitThis | fscrImplicitParents)))
    {
        scope->SetIsObject();
    }

    if (pnodeCatch->sxCatch.pnodeParam->nop == knopParamPattern)
    {
        scope->SetCapturesAll(funcInfo->GetCallsEval() || funcInfo->GetChildCallsEval());
        scope->SetMustInstantiate(scope->GetMustInstantiate() || scope->GetCapturesAll() || funcInfo->IsGlobalFunction());

        Parser::MapBindIdentifier(pnodeCatch->sxCatch.pnodeParam->sxParamPattern.pnode1, [&](ParseNodePtr item)
        {
            Symbol *sym = item->sxVar.sym;
            if (funcInfo->IsGlobalFunction())
            {
                sym->SetIsGlobalCatch(true);
            }

            Assert(sym->GetScopeSlot() == Js::Constants::NoProperty);
            if (sym->NeedsSlotAlloc(funcInfo))
            {
                sym->EnsureScopeSlot(funcInfo);
            }
        });

        // In the case of pattern we will always going to push the scope.
        PushScope(scope);
    }
    else
    {
        Symbol *sym = pnodeCatch->sxCatch.pnodeParam->sxPid.sym;

        // Catch object is stored in the catch scope if there may be an ambiguous lookup or a var declaration that hides it.
        scope->SetCapturesAll(funcInfo->GetCallsEval() || funcInfo->GetChildCallsEval() || sym->GetHasNonLocalReference());
        scope->SetMustInstantiate(scope->GetCapturesAll() || funcInfo->IsGlobalFunction() || currentScope != funcInfo->GetBodyScope());

        if (funcInfo->IsGlobalFunction())
        {
            sym->SetIsGlobalCatch(true);
        }

        if (scope->GetMustInstantiate())
        {
            // Since there is only one symbol we are pushing to slot.
            // Also in order to make IsInSlot to return true - forcing the sym-has-non-local-reference.
            sym->SetHasNonLocalReference(true, this);
            sym->EnsureScopeSlot(funcInfo);

            PushScope(scope);
        }
        else
        {
            // Add it to the parent function's scope and treat it like any other local.
            // We can only do this if we don't need to get the symbol from a slot, though, because adding it to the
            // parent's scope object on entry to the catch could re-size the slot array.
            funcInfo->bodyScope->AddSymbol(sym);
        }
    }
}

void ByteCodeGenerator::EndEmitCatch(ParseNode *pnodeCatch)
{
    Assert(pnodeCatch->nop == knopCatch);
    if (pnodeCatch->sxCatch.scope->GetMustInstantiate() || pnodeCatch->sxCatch.pnodeParam->nop == knopParamPattern)
    {
        Assert(currentScope == pnodeCatch->sxCatch.scope);
        PopScope();
    }
}

void ByteCodeGenerator::StartEmitBlock(ParseNode *pnodeBlock)
{
    if (!BlockHasOwnScope(pnodeBlock, this))
    {
        return;
    }

    Assert(pnodeBlock->nop == knopBlock);

    PushBlock(pnodeBlock);

    Scope *scope = pnodeBlock->sxBlock.scope;
    if (pnodeBlock->sxBlock.GetCallsEval() || pnodeBlock->sxBlock.GetChildCallsEval() || (this->flags & (fscrEval | fscrImplicitThis | fscrImplicitParents)))
    {
        Assert(scope->GetIsObject());
    }

    // TODO: Consider nested deferred parsing.
    if (scope->GetMustInstantiate())
    {
        FuncInfo *funcInfo = scope->GetFunc();
        if (scope->IsGlobalEvalBlockScope() && funcInfo->isThisLexicallyCaptured)
        {
            funcInfo->EnsureThisScopeSlot();
        }
        this->EnsureFncScopeSlots(pnodeBlock->sxBlock.pnodeScopes, funcInfo);
        this->EnsureLetConstScopeSlots(pnodeBlock, funcInfo);
        PushScope(scope);
    }
}

void ByteCodeGenerator::EndEmitBlock(ParseNode *pnodeBlock)
{
    if (!BlockHasOwnScope(pnodeBlock, this))
    {
        return;
    }

    Assert(pnodeBlock->nop == knopBlock);

    Scope *scope = pnodeBlock->sxBlock.scope;
    if (scope && scope->GetMustInstantiate())
    {
        Assert(currentScope == pnodeBlock->sxBlock.scope);
        PopScope();
    }

    PopBlock();
}

void ByteCodeGenerator::StartEmitWith(ParseNode *pnodeWith)
{
    Assert(pnodeWith->nop == knopWith);

    Scope *scope = pnodeWith->sxWith.scope;

    Assert(scope->GetIsObject());

    PushScope(scope);
}

void ByteCodeGenerator::EndEmitWith(ParseNode *pnodeWith)
{
    Assert(pnodeWith->nop == knopWith);
    Assert(currentScope == pnodeWith->sxWith.scope);

    PopScope();
}

Js::RegSlot ByteCodeGenerator::PrependLocalScopes(Js::RegSlot evalEnv, Js::RegSlot tempLoc, FuncInfo *funcInfo)
{
    Scope *currScope = this->currentScope;
    Scope *funcScope = funcInfo->GetBodyScope();

    if (currScope == funcScope)
    {
        return evalEnv;
    }

    bool acquireTempLoc = tempLoc == Js::Constants::NoRegister;
    if (acquireTempLoc)
    {
        tempLoc = funcInfo->AcquireTmpRegister();
    }

    // The with/catch objects must be prepended to the environment we pass to eval() or to a func declared inside with,
    // but the list must first be reversed so that innermost scopes appear first in the list.
    while (currScope != funcScope)
    {
        Scope *innerScope;
        for (innerScope = currScope; innerScope->GetEnclosingScope() != funcScope; innerScope = innerScope->GetEnclosingScope())
            ;
        if (innerScope->GetMustInstantiate())
        {
            if (!innerScope->HasInnerScopeIndex())
            {
                if (evalEnv == funcInfo->GetEnvRegister() || evalEnv == funcInfo->frameDisplayRegister)
                {
                    this->m_writer.Reg2(Js::OpCode::LdInnerFrameDisplayNoParent, tempLoc, innerScope->GetLocation());
                }
                else
                {
                    this->m_writer.Reg3(Js::OpCode::LdInnerFrameDisplay, tempLoc, innerScope->GetLocation(), evalEnv);
                }
            }
            else
            {
                if (evalEnv == funcInfo->GetEnvRegister() || evalEnv == funcInfo->frameDisplayRegister)
                {
                    this->m_writer.Reg1Unsigned1(Js::OpCode::LdIndexedFrameDisplayNoParent, tempLoc, innerScope->GetInnerScopeIndex());
                }
                else
                {
                    this->m_writer.Reg2Int1(Js::OpCode::LdIndexedFrameDisplay, tempLoc, evalEnv, innerScope->GetInnerScopeIndex());
                }
            }
            evalEnv = tempLoc;
        }
        funcScope = innerScope;
    }

    if (acquireTempLoc)
    {
        funcInfo->ReleaseTmpRegister(tempLoc);
    }
    return evalEnv;
}

void ByteCodeGenerator::EmitLoadInstance(Symbol *sym, IdentPtr pid, Js::RegSlot *pThisLocation, Js::RegSlot *pInstLocation, FuncInfo *funcInfo)
{
    Js::ByteCodeLabel doneLabel = 0;
    bool fLabelDefined = false;
    Js::RegSlot scopeLocation = Js::Constants::NoRegister;
    Js::RegSlot thisLocation = *pThisLocation;
    Js::RegSlot instLocation = *pInstLocation;
    Js::PropertyId envIndex = -1;
    Scope *scope = nullptr;
    Scope *symScope = sym ? sym->GetScope() : this->globalScope;
    Assert(symScope);

    for (;;)
    {
        scope = this->FindScopeForSym(symScope, scope, &envIndex, funcInfo);
        if (scope == this->globalScope)
        {
            break;
        }

        if (scope != symScope)
        {
            // We're not sure where the function is (eval/with/etc).
            // So we're going to need registers to hold the instance where we (dynamically) find
            // the function, and possibly to hold the "this" pointer we will pass to it.
            // Assign them here so that they can't overlap with the scopeLocation assigned below.
            // Otherwise we wind up with temp lifetime confusion in the IRBuilder. (Win8 281689)
            if (instLocation == Js::Constants::NoRegister)
            {
                instLocation = funcInfo->AcquireTmpRegister();
                // The "this" pointer will not be the same as the instance, so give it its own register.
                thisLocation = funcInfo->AcquireTmpRegister();
            }
        }

        if (envIndex == -1)
        {
            Assert(funcInfo == scope->GetFunc());
            scopeLocation = scope->GetLocation();
        }

        if (scope == symScope)
        {
            break;
        }

        // Found a scope to which the property may have been added.
        Assert(scope && scope->GetIsDynamic());

        if (!fLabelDefined)
        {
            fLabelDefined = true;
            doneLabel = this->m_writer.DefineLabel();
        }

        Js::ByteCodeLabel nextLabel = this->m_writer.DefineLabel();
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();

        bool unwrapWithObj = scope->GetScopeType() == ScopeType_With && scriptContext->GetConfig()->IsES6UnscopablesEnabled();
        if (envIndex != -1)
        {
            this->m_writer.BrEnvProperty(
                Js::OpCode::BrOnNoEnvProperty, nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId),
                envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var));

            Js::RegSlot tmpReg = funcInfo->AcquireTmpRegister();

            this->m_writer.SlotI1(Js::OpCode::LdEnvObj, tmpReg,
                                  envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var));

            Js::OpCode op = unwrapWithObj ? Js::OpCode::UnwrapWithObj : Js::OpCode::Ld_A;

            this->m_writer.Reg2(op, instLocation, tmpReg);
            if (thisLocation != Js::Constants::NoRegister)
            {
                this->m_writer.Reg2(op, thisLocation, tmpReg);
            }

            funcInfo->ReleaseTmpRegister(tmpReg);
        }
        else if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
        {
            this->m_writer.BrLocalProperty(Js::OpCode::BrOnNoLocalProperty, nextLabel,
                                           funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Assert(!unwrapWithObj);
            this->m_writer.Reg1(Js::OpCode::LdLocalObj, instLocation);
            if (thisLocation != Js::Constants::NoRegister)
            {
                this->m_writer.Reg1(Js::OpCode::LdLocalObj, thisLocation);
            }
        }
        else
        {
            this->m_writer.BrProperty(Js::OpCode::BrOnNoProperty, nextLabel, scopeLocation,
                                      funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Js::OpCode op = unwrapWithObj ? Js::OpCode::UnwrapWithObj : Js::OpCode::Ld_A;
            this->m_writer.Reg2(op, instLocation, scopeLocation);
            if (thisLocation != Js::Constants::NoRegister)
            {
                this->m_writer.Reg2(op, thisLocation, scopeLocation);
            }
        }

        this->m_writer.Br(doneLabel);
        this->m_writer.MarkLabel(nextLabel);
    }

    if (sym == nullptr || sym->GetIsGlobal())
    {
        if (this->flags & (fscrEval | fscrImplicitThis | fscrImplicitParents))
        {
            // Load of a symbol with unknown scope from within eval.
            // Get it from the closure environment.
            if (instLocation == Js::Constants::NoRegister)
            {
                instLocation = funcInfo->AcquireTmpRegister();
            }

            // TODO: It should be possible to avoid this double call to ScopedLdInst by having it return both
            // results at once. The reason for the uncertainty here is that we don't know whether the callee
            // belongs to a "with" object. If it does, we have to pass the "with" object as "this"; in all other
            // cases, we pass "undefined". For now, there are apparently no significant performance issues.
            Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();

            if (thisLocation == Js::Constants::NoRegister)
            {
                thisLocation = funcInfo->AcquireTmpRegister();
            }
            this->m_writer.ScopedProperty2(Js::OpCode::ScopedLdInst, instLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId), thisLocation);
        }
        else
        {
            if (instLocation == Js::Constants::NoRegister)
            {
                instLocation = ByteCodeGenerator::RootObjectRegister;
            }
            else
            {
                this->m_writer.Reg2(Js::OpCode::Ld_A, instLocation, ByteCodeGenerator::RootObjectRegister);
            }

            if (thisLocation == Js::Constants::NoRegister)
            {
                thisLocation = funcInfo->undefinedConstantRegister;
            }
            else
            {
                this->m_writer.Reg2(Js::OpCode::Ld_A, thisLocation, funcInfo->undefinedConstantRegister);
            }
        }
    }
    else if (instLocation != Js::Constants::NoRegister)
    {
        if (envIndex != -1)
        {
            this->m_writer.SlotI1(Js::OpCode::LdEnvObj, instLocation,
                                  envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var));
        }
        else if (scope->HasInnerScopeIndex())
        {
            this->m_writer.Reg1Unsigned1(Js::OpCode::LdInnerScope, instLocation, scope->GetInnerScopeIndex());
        }
        else if (symScope != funcInfo->GetBodyScope())
        {
            this->m_writer.Reg2(Js::OpCode::Ld_A, instLocation, scopeLocation);
        }
        else
        {
            this->m_writer.Reg1(Js::OpCode::LdLocalObj, instLocation);
        }

        if (thisLocation != Js::Constants::NoRegister)
        {
            this->m_writer.Reg2(Js::OpCode::Ld_A, thisLocation, funcInfo->undefinedConstantRegister);
        }
        else
        {
            thisLocation = funcInfo->undefinedConstantRegister;
        }
    }

    *pThisLocation = thisLocation;
    *pInstLocation = instLocation;

    if (fLabelDefined)
    {
        this->m_writer.MarkLabel(doneLabel);
    }
}

void ByteCodeGenerator::EmitGlobalFncDeclInit(Js::RegSlot rhsLocation, Js::PropertyId propertyId, FuncInfo * funcInfo)
{
    // Note: declared variables and assignments in the global function go to the root object directly.
    if (this->flags & fscrEval)
    {
        // Func decl's always get their init values before any use, so we don't pre-initialize the property to undef.
        // That means that we have to use ScopedInitFld so that we initialize the property on the right instance
        // even if the instance doesn't have the property yet (i.e., collapse the init-to-undef and the store
        // into one operation). See WOOB 1121763 and 1120973.
        this->m_writer.ScopedProperty(Js::OpCode::ScopedInitFunc, rhsLocation,
                                      funcInfo->FindOrAddReferencedPropertyId(propertyId));
    }
    else
    {
        this->EmitPatchableRootProperty(Js::OpCode::InitRootFld, rhsLocation, propertyId, false, true, funcInfo);
    }
}

void
ByteCodeGenerator::EmitPatchableRootProperty(Js::OpCode opcode,
    Js::RegSlot regSlot, Js::PropertyId propertyId, bool isLoadMethod, bool isStore, FuncInfo * funcInfo)
{
    uint cacheId = funcInfo->FindOrAddRootObjectInlineCacheId(propertyId, isLoadMethod, isStore);
    this->m_writer.PatchableRootProperty(opcode, regSlot, cacheId, isLoadMethod, isStore);
}

void ByteCodeGenerator::EmitLocalPropInit(Js::RegSlot rhsLocation, Symbol *sym, FuncInfo *funcInfo)
{
    Scope *scope = sym->GetScope();

    // Check consistency of sym->IsInSlot.
    Assert(sym->NeedsSlotAlloc(funcInfo) || sym->GetScopeSlot() == Js::Constants::NoProperty);

    // Arrived at the scope in which the property was defined.
    if (sym->NeedsSlotAlloc(funcInfo))
    {
        // The property is in memory rather than register. We'll have to load it from the slots.
        if (scope->GetIsObject())
        {
            Assert(!this->TopFuncInfo()->GetParsedFunctionBody()->DoStackNestedFunc());
            Js::PropertyId propertyId = sym->EnsurePosition(this);
            Js::RegSlot objReg;
            if (scope->HasInnerScopeIndex())
            {
                objReg = funcInfo->InnerScopeToRegSlot(scope);
            }
            else
            {
                objReg = scope->GetLocation();
            }
            uint cacheId = funcInfo->FindOrAddInlineCacheId(objReg, propertyId, false, true);
            Js::OpCode op = this->GetInitFldOp(scope, objReg, funcInfo, sym->GetIsNonSimpleParameter());
            if (objReg != Js::Constants::NoRegister && objReg == funcInfo->frameObjRegister)
            {
                this->m_writer.ElementP(op, rhsLocation, cacheId);
            }
            else if (scope->HasInnerScopeIndex())
            {
                this->m_writer.ElementPIndexed(op, rhsLocation, scope->GetInnerScopeIndex(), cacheId);
            }
            else
            {
                this->m_writer.PatchableProperty(op, rhsLocation, scope->GetLocation(), cacheId);
            }
        }
        else
        {
            // Make sure the property has a slot. This will bump up the size of the slot array if necessary.
            Js::PropertyId slot = sym->EnsureScopeSlot(funcInfo);
            Js::RegSlot slotReg = scope->GetCanMerge() ? funcInfo->frameSlotsRegister : scope->GetLocation();
            // Now store the property to its slot.
            Js::OpCode op = this->GetStSlotOp(scope, -1, slotReg, false, funcInfo);

            if (slotReg != Js::Constants::NoRegister && slotReg == funcInfo->frameSlotsRegister)
            {
                this->m_writer.SlotI1(op, rhsLocation, slot + Js::ScopeSlots::FirstSlotIndex);
            }
            else
            {
                this->m_writer.SlotI2(op, rhsLocation, scope->GetInnerScopeIndex(), slot + Js::ScopeSlots::FirstSlotIndex);
            }
        }
    }
    if (sym->GetLocation() != Js::Constants::NoRegister && rhsLocation != sym->GetLocation())
    {
        this->m_writer.Reg2(Js::OpCode::Ld_A, sym->GetLocation(), rhsLocation);
    }
}

Js::OpCode
ByteCodeGenerator::GetStSlotOp(Scope *scope, int envIndex, Js::RegSlot scopeLocation, bool chkBlockVar, FuncInfo *funcInfo)
{
    Js::OpCode op;

    if (envIndex != -1)
    {
        if (scope->GetIsObject())
        {
            op = Js::OpCode::StEnvObjSlot;
        }
        else
        {
            op = Js::OpCode::StEnvSlot;
        }
    }
    else if (scopeLocation != Js::Constants::NoRegister &&
             scopeLocation == funcInfo->frameSlotsRegister)
    {
        op = Js::OpCode::StLocalSlot;
    }
    else if (scopeLocation != Js::Constants::NoRegister &&
             scopeLocation == funcInfo->frameObjRegister)
    {
        op = Js::OpCode::StLocalObjSlot;
    }
    else
    {
        Assert(scope->HasInnerScopeIndex());
        if (scope->GetIsObject())
        {
            op = Js::OpCode::StInnerObjSlot;
        }
        else
        {
            op = Js::OpCode::StInnerSlot;
        }
    }

    if (chkBlockVar)
    {
        op = this->ToChkUndeclOp(op);
    }

    return op;
}

Js::OpCode
ByteCodeGenerator::GetInitFldOp(Scope *scope, Js::RegSlot scopeLocation, FuncInfo *funcInfo, bool letDecl)
{
    Js::OpCode op;

    if (scopeLocation != Js::Constants::NoRegister &&
        scopeLocation == funcInfo->frameObjRegister)
    {
        op = letDecl ? Js::OpCode::InitLocalLetFld : Js::OpCode::InitLocalFld;
    }
    else if (scope->HasInnerScopeIndex())
    {
        op = letDecl ? Js::OpCode::InitInnerLetFld : Js::OpCode::InitInnerFld;
    }
    else
    {
        op = letDecl ? Js::OpCode::InitLetFld : Js::OpCode::InitFld;
    }

    return op;
}

void ByteCodeGenerator::EmitPropStore(Js::RegSlot rhsLocation, Symbol *sym, IdentPtr pid, FuncInfo *funcInfo, bool isLetDecl, bool isConstDecl, bool isFncDeclVar)
{
    Js::ByteCodeLabel doneLabel = 0;
    bool fLabelDefined = false;
    Js::PropertyId envIndex = -1;
    Scope *symScope = sym == nullptr || sym->GetIsGlobal() ? this->globalScope : sym->GetScope();
    Assert(symScope);
    // isFncDeclVar denotes that the symbol being stored to here is the var
    // binding of a function declaration and we know we want to store directly
    // to it, skipping over any dynamic scopes that may lie in between.
    Scope *scope = isFncDeclVar ? symScope : nullptr;
    Js::RegSlot scopeLocation = isFncDeclVar ? scope->GetLocation() : Js::Constants::NoRegister;
    bool scopeAcquired = false;
    Js::OpCode op;

    while (!isFncDeclVar)
    {
        scope = this->FindScopeForSym(symScope, scope, &envIndex, funcInfo);
        if (scope == this->globalScope)
        {
            break;
        }
        if (envIndex == -1)
        {
            Assert(funcInfo == scope->GetFunc());
            scopeLocation = scope->GetLocation();
        }

        if (scope == symScope)
        {
            break;
        }

        // Found a scope to which the property may have been added.
        Assert(scope && scope->GetIsDynamic());

        if (!fLabelDefined)
        {
            fLabelDefined = true;
            doneLabel = this->m_writer.DefineLabel();
        }
        Js::ByteCodeLabel nextLabel = this->m_writer.DefineLabel();
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();

        Js::RegSlot unwrappedScopeLocation = scopeLocation;
        bool unwrapWithObj = scope->GetScopeType() == ScopeType_With && scriptContext->GetConfig()->IsES6UnscopablesEnabled();
        if (envIndex != -1)
        {
            this->m_writer.BrEnvProperty(
                Js::OpCode::BrOnNoEnvProperty,
                nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId),
                envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var));

            Js::RegSlot instLocation = funcInfo->AcquireTmpRegister();

            this->m_writer.SlotI1(
                Js::OpCode::LdEnvObj,
                instLocation,
                envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var));

            if (unwrapWithObj)
            {
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, instLocation, instLocation);
            }

            this->m_writer.PatchableProperty(
                Js::OpCode::StFld,
                rhsLocation,
                instLocation,
                funcInfo->FindOrAddInlineCacheId(instLocation, propertyId, false, true));

            funcInfo->ReleaseTmpRegister(instLocation);
        }
        else if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
        {
            this->m_writer.BrLocalProperty(Js::OpCode::BrOnNoLocalProperty, nextLabel,
                                           funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Assert(!unwrapWithObj);
            this->m_writer.ElementP(Js::OpCode::StLocalFld, rhsLocation,
                                    funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, true));
        }
        else
        {
            this->m_writer.BrProperty(Js::OpCode::BrOnNoProperty, nextLabel, scopeLocation,
                                      funcInfo->FindOrAddReferencedPropertyId(propertyId));

            if (unwrapWithObj)
            {
                unwrappedScopeLocation = funcInfo->AcquireTmpRegister();
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, unwrappedScopeLocation, scopeLocation);
                scopeLocation = unwrappedScopeLocation;
            }

            uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, true);
            this->m_writer.PatchableProperty(Js::OpCode::StFld, rhsLocation, scopeLocation, cacheId);

            if (unwrapWithObj)
            {
                funcInfo->ReleaseTmpRegister(unwrappedScopeLocation);
            }
        }

        this->m_writer.Br(doneLabel);
        this->m_writer.MarkLabel(nextLabel);
    }

    // Arrived at the scope in which the property was defined.
    if (sym == nullptr || sym->GetIsGlobal())
    {
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();
        if (this->flags & fscrEval)
        {
            if (funcInfo->byteCodeFunction->GetIsStrictMode() && funcInfo->IsGlobalFunction())
            {
                uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->frameDisplayRegister, propertyId, false, true);
                this->m_writer.ElementP(GetScopedStFldOpCode(funcInfo), rhsLocation, cacheId);
            }
            else
            {
                uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->GetEnvRegister(), propertyId, false, true);
                bool isConsoleScopeLetConst = this->IsConsoleScopeEval() && (isLetDecl || isConstDecl);
                // In "eval", store to a symbol with unknown scope goes through the closure environment.
                this->m_writer.ElementP(GetScopedStFldOpCode(funcInfo, isConsoleScopeLetConst), rhsLocation, cacheId);
            }
        }
        else if (this->flags & (fscrImplicitThis | fscrImplicitParents))
        {
            uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->GetEnvRegister(), propertyId, false, true);

            // In "eval", store to a symbol with unknown scope goes through the closure environment.
            this->m_writer.ElementP(GetScopedStFldOpCode(funcInfo), rhsLocation, cacheId);
        }
        else
        {
            this->EmitPatchableRootProperty(GetStFldOpCode(funcInfo, true, isLetDecl, isConstDecl, false), rhsLocation, propertyId, false, true, funcInfo);
        }
    }
    else if (sym->GetFuncExpr())
    {
        // Store to function expr variable.

        // strict mode: we need to throw type error
        if (funcInfo->byteCodeFunction->GetIsStrictMode())
        {
            // Note that in this case the sym's location belongs to the parent function, so we can't use it.
            // It doesn't matter which register we use, as long as it's valid for this function.
            this->m_writer.W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(JSERR_CantAssignToReadOnly));
        }
    }
    else if (sym->IsInSlot(funcInfo) || envIndex != -1)
    {
        if (!isConstDecl && sym->GetDecl() && sym->GetDecl()->nop == knopConstDecl)
        {
            // This is a case where const reassignment can't be proven statically (e.g., eval, with) so
            // we have to catch it at runtime.
            this->m_writer.W1(
                Js::OpCode::RuntimeReferenceError, SCODE_CODE(ERRAssignmentToConst));
        }
        // Make sure the property has a slot. This will bump up the size of the slot array if necessary.
        Js::PropertyId slot = sym->EnsureScopeSlot(funcInfo);
        bool chkBlockVar = !isLetDecl && !isConstDecl && NeedCheckBlockVar(sym, scope, funcInfo);

        // The property is in memory rather than register. We'll have to load it from the slots.
        op = this->GetStSlotOp(scope, envIndex, scopeLocation, chkBlockVar, funcInfo);

        if (envIndex != -1)
        {
            this->m_writer.SlotI2(op, rhsLocation,
                                  envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var),
                                  slot + (sym->GetScope()->GetIsObject()? 0 : Js::ScopeSlots::FirstSlotIndex));
        }
        else if (scopeLocation != Js::Constants::NoRegister &&
                 (scopeLocation == funcInfo->frameSlotsRegister || scopeLocation == funcInfo->frameObjRegister))
        {
            this->m_writer.SlotI1(op, rhsLocation,
                                  slot + (sym->GetScope()->GetIsObject()? 0 : Js::ScopeSlots::FirstSlotIndex));
        }
        else
        {
            Assert(scope->HasInnerScopeIndex());
            this->m_writer.SlotI2(op, rhsLocation, scope->GetInnerScopeIndex(),
                slot + (sym->GetScope()->GetIsObject()? 0 : Js::ScopeSlots::FirstSlotIndex));
        }

        if (this->ShouldTrackDebuggerMetadata() && (isLetDecl || isConstDecl))
        {
            Js::PropertyId location = scope->GetIsObject() ? sym->GetLocation() : slot;
            this->UpdateDebuggerPropertyInitializationOffset(location, sym->GetPosition(), false);
        }
    }
    else if (isConstDecl)
    {
        this->m_writer.Reg2(Js::OpCode::InitConst, sym->GetLocation(), rhsLocation);

        if (this->ShouldTrackDebuggerMetadata())
        {
            this->UpdateDebuggerPropertyInitializationOffset(sym->GetLocation(), sym->GetPosition());
        }
    }
    else
    {
        if (!isConstDecl && sym->GetDecl() && sym->GetDecl()->nop == knopConstDecl)
        {
            // This is a case where const reassignment can't be proven statically (e.g., eval, with) so
            // we have to catch it at runtime.
            this->m_writer.W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(ERRAssignmentToConst));
        }
        if (rhsLocation != sym->GetLocation())
        {
            this->m_writer.Reg2(Js::OpCode::Ld_A, sym->GetLocation(), rhsLocation);

            if (this->ShouldTrackDebuggerMetadata() && isLetDecl)
            {
                this->UpdateDebuggerPropertyInitializationOffset(sym->GetLocation(), sym->GetPosition());
            }
        }
    }
    if (fLabelDefined)
    {
        this->m_writer.MarkLabel(doneLabel);
    }

    if (scopeAcquired)
    {
        funcInfo->ReleaseTmpRegister(scopeLocation);
    }
}

Js::OpCode
ByteCodeGenerator::GetLdSlotOp(Scope *scope, int envIndex, Js::RegSlot scopeLocation, FuncInfo *funcInfo)
{
    Js::OpCode op;

    if (envIndex != -1)
    {
        if (scope->GetIsObject())
        {
            op = Js::OpCode::LdEnvObjSlot;
        }
        else
        {
            op = Js::OpCode::LdEnvSlot;
        }
    }
    else if (scopeLocation != Js::Constants::NoRegister &&
             scopeLocation == funcInfo->frameSlotsRegister)
    {
        op = Js::OpCode::LdLocalSlot;
    }
    else if (scopeLocation != Js::Constants::NoRegister &&
             scopeLocation == funcInfo->frameObjRegister)
    {
        op = Js::OpCode::LdLocalObjSlot;
    }
    else if (scope->HasInnerScopeIndex())
    {
        if (scope->GetIsObject())
        {
            op = Js::OpCode::LdInnerObjSlot;
        }
        else
        {
            op = Js::OpCode::LdInnerSlot;
        }
    }
    else
    {
        Assert(scope->GetIsObject());
        op = Js::OpCode::LdObjSlot;
    }

    return op;
}

void ByteCodeGenerator::EmitPropLoad(Js::RegSlot lhsLocation, Symbol *sym, IdentPtr pid, FuncInfo *funcInfo)
{
    // If sym belongs to a parent frame, get it from the closure environment.
    // If it belongs to this func, but there's a non-local reference, get it from the heap-allocated frame.
    // (TODO: optimize this by getting the sym from its normal location if there are no non-local defs.)
    // Otherwise, just copy the value to the lhsLocation.

    Js::ByteCodeLabel doneLabel = 0;
    bool fLabelDefined = false;
    Js::RegSlot scopeLocation = Js::Constants::NoRegister;
    Js::PropertyId envIndex = -1;
    Scope *scope = nullptr;
    Scope *symScope = sym ? sym->GetScope() : this->globalScope;
    Assert(symScope);

    for (;;)
    {
        scope = this->FindScopeForSym(symScope, scope, &envIndex, funcInfo);
        if (scope == this->globalScope)
        {
            break;
        }

        scopeLocation = scope->GetLocation();

        if (scope == symScope)
        {
            break;
        }

        // Found a scope to which the property may have been added.
        Assert(scope && scope->GetIsDynamic());

        if (!fLabelDefined)
        {
            fLabelDefined = true;
            doneLabel = this->m_writer.DefineLabel();
        }

        Js::ByteCodeLabel nextLabel = this->m_writer.DefineLabel();
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();

        Js::RegSlot unwrappedScopeLocation = Js::Constants::NoRegister;
        bool unwrapWithObj = scope->GetScopeType() == ScopeType_With && scriptContext->GetConfig()->IsES6UnscopablesEnabled();
        if (envIndex != -1)
        {
            this->m_writer.BrEnvProperty(
                Js::OpCode::BrOnNoEnvProperty,
                nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId),
                envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var));

            Js::RegSlot instLocation = funcInfo->AcquireTmpRegister();

            this->m_writer.SlotI1(
                Js::OpCode::LdEnvObj,
                instLocation,
                envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var));

            if (unwrapWithObj)
            {
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, instLocation, instLocation);
            }

            this->m_writer.PatchableProperty(
                Js::OpCode::LdFld,
                lhsLocation,
                instLocation,
                funcInfo->FindOrAddInlineCacheId(instLocation, propertyId, false, false));

            funcInfo->ReleaseTmpRegister(instLocation);
        }
        else if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
        {
            this->m_writer.BrLocalProperty(Js::OpCode::BrOnNoLocalProperty, nextLabel,
                                           funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Assert(!unwrapWithObj);
            this->m_writer.ElementP(Js::OpCode::LdLocalFld, lhsLocation,
                                    funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, false));
        }
        else
        {
            this->m_writer.BrProperty(Js::OpCode::BrOnNoProperty, nextLabel, scopeLocation,
                                      funcInfo->FindOrAddReferencedPropertyId(propertyId));

            if (unwrapWithObj)
            {
                unwrappedScopeLocation = funcInfo->AcquireTmpRegister();
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, unwrappedScopeLocation, scopeLocation);
                scopeLocation = unwrappedScopeLocation;
            }

            uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, false);
            this->m_writer.PatchableProperty(Js::OpCode::LdFld, lhsLocation, scopeLocation, cacheId);

            if (unwrapWithObj)
            {
                funcInfo->ReleaseTmpRegister(unwrappedScopeLocation);
            }
        }

        this->m_writer.Br(doneLabel);
        this->m_writer.MarkLabel(nextLabel);
    }

    // Arrived at the scope in which the property was defined.
    if (sym && sym->GetNeedDeclaration() && scope->GetFunc() == funcInfo)
    {
        // Ensure this symbol has a slot if it needs one.
        if (sym->IsInSlot(funcInfo))
        {
            Js::PropertyId slot = sym->EnsureScopeSlot(funcInfo);
            funcInfo->FindOrAddSlotProfileId(scope, slot);
        }

        EmitUseBeforeDeclarationRuntimeError(this, lhsLocation);
    }
    else if (sym == nullptr || sym->GetIsGlobal())
    {
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();
        if (this->flags & fscrEval)
        {
            if (funcInfo->byteCodeFunction->GetIsStrictMode() && funcInfo->IsGlobalFunction())
            {
                uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->frameDisplayRegister, propertyId, false, false);
                this->m_writer.ElementP(Js::OpCode::ScopedLdFld, lhsLocation, cacheId);
            }
            else
            {
                uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->GetEnvRegister(), propertyId, false, false);

                // Load of a symbol with unknown scope from within eval
                // Get it from the closure environment.
                this->m_writer.ElementP(Js::OpCode::ScopedLdFld, lhsLocation, cacheId);
            }
        }
        else if (this->flags & (fscrImplicitThis | fscrImplicitParents))
        {
            uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->GetEnvRegister(), propertyId, false, false);

            // Load of a symbol with unknown scope from within eval or event handler.
            // Get it from the closure environment.
            this->m_writer.ElementP(Js::OpCode::ScopedLdFld, lhsLocation, cacheId);
        }
        else
        {
            // Special case non-writable built-ins
            // TODO: support non-writable global property in general by detecting what attribute the property have current?
            // But can't be done if we are byte code serialized, because the attribute might be different for use fields
            // next time we run. May want to catch that in the JIT.
            Js::OpCode opcode = Js::OpCode::LdRootFld;

            // These properties are non-writable
            switch (propertyId)
            {
            case Js::PropertyIds::NaN:
                opcode = Js::OpCode::LdNaN;
                break;
            case Js::PropertyIds::Infinity:
                opcode = Js::OpCode::LdInfinity;
                break;
            case Js::PropertyIds::undefined:
                opcode = Js::OpCode::LdUndef;
                break;
            }

            if (opcode == Js::OpCode::LdRootFld)
            {
                this->EmitPatchableRootProperty(Js::OpCode::LdRootFld, lhsLocation, propertyId, false, false, funcInfo);
            }
            else
            {
                this->Writer()->Reg1(opcode, lhsLocation);
            }
        }
    }
    else if (sym->IsInSlot(funcInfo) || envIndex != -1)
    {
        // Make sure the property has a slot. This will bump up the size of the slot array if necessary.
        Js::PropertyId slot = sym->EnsureScopeSlot(funcInfo);
        Js::ProfileId profileId = funcInfo->FindOrAddSlotProfileId(scope, slot);
        bool chkBlockVar = NeedCheckBlockVar(sym, scope, funcInfo);
        Js::OpCode op;

        // Now get the property from its slot.
        op = this->GetLdSlotOp(scope, envIndex, scopeLocation, funcInfo);
        slot = slot + (sym->GetScope()->GetIsObject() ? 0 : Js::ScopeSlots::FirstSlotIndex);

        if (envIndex != -1)
        {
            this->m_writer.SlotI2(op, lhsLocation, envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var), slot, profileId);
        }
        else if (scopeLocation != Js::Constants::NoRegister &&
                 (scopeLocation == funcInfo->frameSlotsRegister || scopeLocation == funcInfo->frameObjRegister))
        {
            this->m_writer.SlotI1(op, lhsLocation, slot, profileId);
        }
        else if (scope->HasInnerScopeIndex())
        {
            this->m_writer.SlotI2(op, lhsLocation, scope->GetInnerScopeIndex(), slot, profileId);
        }
        else
        {
            Assert(scope->GetIsObject());
            this->m_writer.Slot(op, lhsLocation, scopeLocation, slot, profileId);
        }

        if (chkBlockVar)
        {
            this->m_writer.Reg1(Js::OpCode::ChkUndecl, lhsLocation);
        }
    }
    else
    {
        if (lhsLocation != sym->GetLocation())
        {
            this->m_writer.Reg2(Js::OpCode::Ld_A, lhsLocation, sym->GetLocation());
        }
        if (sym->GetIsBlockVar() && ((sym->GetDecl()->nop == knopLetDecl || sym->GetDecl()->nop == knopConstDecl) && sym->GetDecl()->sxVar.isSwitchStmtDecl) && CONFIG_FLAG(TDZ))
        {
            this->m_writer.Reg1(Js::OpCode::ChkUndecl, lhsLocation);
        }
    }

    if (fLabelDefined)
    {
        this->m_writer.MarkLabel(doneLabel);
    }
}

bool ByteCodeGenerator::NeedCheckBlockVar(Symbol* sym, Scope* scope, FuncInfo* funcInfo) const
{
    bool tdz = sym->GetIsBlockVar()
        && (scope->GetFunc() != funcInfo || ((sym->GetDecl()->nop == knopLetDecl || sym->GetDecl()->nop == knopConstDecl) && sym->GetDecl()->sxVar.isSwitchStmtDecl))
        && CONFIG_FLAG(TDZ);

    return tdz || sym->GetIsNonSimpleParameter();
}

void ByteCodeGenerator::EmitPropDelete(Js::RegSlot lhsLocation, Symbol *sym, IdentPtr pid, FuncInfo *funcInfo)
{
    // If sym belongs to a parent frame, delete it from the closure environment.
    // If it belongs to this func, but there's a non-local reference, get it from the heap-allocated frame.
    // (TODO: optimize this by getting the sym from its normal location if there are no non-local defs.)
    // Otherwise, just return false.

    Js::ByteCodeLabel doneLabel = 0;
    bool fLabelDefined = false;
    Js::RegSlot scopeLocation = Js::Constants::NoRegister;
    Js::PropertyId envIndex = -1;
    Scope *scope = nullptr;
    Scope *symScope = sym ? sym->GetScope() : this->globalScope;
    Assert(symScope);

    for (;;)
    {
        scope = this->FindScopeForSym(symScope, scope, &envIndex, funcInfo);
        if (scope == this->globalScope)
        {
            scopeLocation = ByteCodeGenerator::RootObjectRegister;
        }
        else if (envIndex == -1)
        {
            Assert(funcInfo == scope->GetFunc());
            scopeLocation = scope->GetLocation();
        }

        if (scope == symScope)
        {
            break;
        }

        // Found a scope to which the property may have been added.
        Assert(scope && scope->GetIsDynamic());

        if (!fLabelDefined)
        {
            fLabelDefined = true;
            doneLabel = this->m_writer.DefineLabel();
        }

        Js::ByteCodeLabel nextLabel = this->m_writer.DefineLabel();
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();
        bool unwrapWithObj = scope->GetScopeType() == ScopeType_With && scriptContext->GetConfig()->IsES6UnscopablesEnabled();
        if (envIndex != -1)
        {
            this->m_writer.BrEnvProperty(
                Js::OpCode::BrOnNoEnvProperty,
                nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId),
                envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var));

            Js::RegSlot instLocation = funcInfo->AcquireTmpRegister();

            this->m_writer.SlotI1(
                Js::OpCode::LdEnvObj,
                instLocation,
                envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var));

            if (unwrapWithObj)
            {
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, instLocation, instLocation);
            }

            this->m_writer.Property(Js::OpCode::DeleteFld, lhsLocation, instLocation,
                                    funcInfo->FindOrAddReferencedPropertyId(propertyId));

            funcInfo->ReleaseTmpRegister(instLocation);
        }
        else if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
        {
            this->m_writer.BrLocalProperty(Js::OpCode::BrOnNoLocalProperty, nextLabel,
                                           funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Assert(!unwrapWithObj);
            this->m_writer.ElementU(Js::OpCode::DeleteLocalFld, lhsLocation,
                                    funcInfo->FindOrAddReferencedPropertyId(propertyId));
        }
        else
        {
            this->m_writer.BrProperty(Js::OpCode::BrOnNoProperty, nextLabel, scopeLocation,
                                      funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Js::RegSlot unwrappedScopeLocation = Js::Constants::NoRegister;
            if (unwrapWithObj)
            {
                unwrappedScopeLocation = funcInfo->AcquireTmpRegister();
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, unwrappedScopeLocation, scopeLocation);
                scopeLocation = unwrappedScopeLocation;
            }

            this->m_writer.Property(Js::OpCode::DeleteFld, lhsLocation, scopeLocation,
                                    funcInfo->FindOrAddReferencedPropertyId(propertyId));

            if (unwrapWithObj)
            {
                funcInfo->ReleaseTmpRegister(unwrappedScopeLocation);
            }
        }

        this->m_writer.Br(doneLabel);
        this->m_writer.MarkLabel(nextLabel);
    }

    // Arrived at the scope in which the property was defined.
    if (sym == nullptr || sym->GetIsGlobal())
    {
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();
        if (this->flags & (fscrEval | fscrImplicitThis | fscrImplicitParents))
        {
            this->m_writer.ScopedProperty(Js::OpCode::ScopedDeleteFld, lhsLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));
        }
        else
        {
            this->m_writer.Property(Js::OpCode::DeleteRootFld, lhsLocation, ByteCodeGenerator::RootObjectRegister,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));
        }
    }
    else
    {
        // The delete will look like a non-local reference, so make sure a slot is reserved.
        sym->EnsureScopeSlot(funcInfo);
        this->m_writer.Reg1(Js::OpCode::LdFalse, lhsLocation);
    }

    if (fLabelDefined)
    {
        this->m_writer.MarkLabel(doneLabel);
    }
}

void ByteCodeGenerator::EmitTypeOfFld(FuncInfo * funcInfo, Js::PropertyId propertyId, Js::RegSlot value, Js::RegSlot instance, Js::OpCode ldFldOp)
{

    uint cacheId;
    Js::RegSlot tmpReg = funcInfo->AcquireTmpRegister();
    switch (ldFldOp)
    {
    case Js::OpCode::LdRootFldForTypeOf:
        cacheId = funcInfo->FindOrAddRootObjectInlineCacheId(propertyId, false, false);
        this->Writer()->PatchableRootProperty(ldFldOp, tmpReg, cacheId, false, false);
        break;

    case Js::OpCode::LdLocalFld:
    case Js::OpCode::ScopedLdFldForTypeOf:
        cacheId = funcInfo->FindOrAddInlineCacheId(instance, propertyId, false, false);
        this->Writer()->ElementP(ldFldOp, tmpReg, cacheId);
        break;
        
    default:
        cacheId = funcInfo->FindOrAddInlineCacheId(instance, propertyId, false, false);
        this->Writer()->PatchableProperty(ldFldOp, tmpReg, instance, cacheId);
        break;
    }

    this->Writer()->Reg2(Js::OpCode::Typeof, value, tmpReg);
    funcInfo->ReleaseTmpRegister(tmpReg);
}

void ByteCodeGenerator::EmitPropTypeof(Js::RegSlot lhsLocation, Symbol *sym, IdentPtr pid, FuncInfo *funcInfo)
{
    // If sym belongs to a parent frame, delete it from the closure environment.
    // If it belongs to this func, but there's a non-local reference, get it from the heap-allocated frame.
    // (TODO: optimize this by getting the sym from its normal location if there are no non-local defs.)
    // Otherwise, just return false

    Js::ByteCodeLabel doneLabel = 0;
    bool fLabelDefined = false;
    Js::RegSlot scopeLocation = Js::Constants::NoRegister;
    Js::PropertyId envIndex = -1;
    Scope *scope = nullptr;
    Scope *symScope = sym ? sym->GetScope() : this->globalScope;
    Assert(symScope);

    for (;;)
    {
        scope = this->FindScopeForSym(symScope, scope, &envIndex, funcInfo);
        if (scope == this->globalScope)
        {
            scopeLocation = ByteCodeGenerator::RootObjectRegister;
        }
        else if (envIndex == -1)
        {
            Assert(funcInfo == scope->GetFunc());
            scopeLocation = scope->GetLocation();
        }

        if (scope == symScope)
        {
            break;
        }

        // Found a scope to which the property may have been added.
        Assert(scope && scope->GetIsDynamic());

        if (!fLabelDefined)
        {
            fLabelDefined = true;
            doneLabel = this->m_writer.DefineLabel();
        }

        Js::ByteCodeLabel nextLabel = this->m_writer.DefineLabel();
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();

        bool unwrapWithObj = scope->GetScopeType() == ScopeType_With && scriptContext->GetConfig()->IsES6UnscopablesEnabled();
        if (envIndex != -1)
        {
            this->m_writer.BrEnvProperty(Js::OpCode::BrOnNoEnvProperty, nextLabel,
                                         funcInfo->FindOrAddReferencedPropertyId(propertyId),
                                         envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var));

            Js::RegSlot instLocation = funcInfo->AcquireTmpRegister();

            this->m_writer.SlotI1(Js::OpCode::LdEnvObj,
                                  instLocation,
                                  envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var));

            if (unwrapWithObj)
            {
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, instLocation, instLocation);
            }

            this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, instLocation, Js::OpCode::LdFldForTypeOf);

            funcInfo->ReleaseTmpRegister(instLocation);
        }
        else if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
        {
            this->m_writer.BrLocalProperty(Js::OpCode::BrOnNoLocalProperty, nextLabel,
                                           funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Assert(!unwrapWithObj);
            this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, scopeLocation, Js::OpCode::LdLocalFld);
        }
        else
        {
            this->m_writer.BrProperty(Js::OpCode::BrOnNoProperty, nextLabel, scopeLocation,
                                      funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Js::RegSlot unwrappedScopeLocation = Js::Constants::NoRegister;
            if (unwrapWithObj)
            {
                unwrappedScopeLocation = funcInfo->AcquireTmpRegister();
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, unwrappedScopeLocation, scopeLocation);
                scopeLocation = unwrappedScopeLocation;
            }

            this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, scopeLocation, Js::OpCode::LdFldForTypeOf);

            if (unwrapWithObj)
            {
                funcInfo->ReleaseTmpRegister(unwrappedScopeLocation);
            }
        }

        this->m_writer.Br(doneLabel);
        this->m_writer.MarkLabel(nextLabel);
    }

    // Arrived at the scope in which the property was defined.
    if (sym && sym->GetNeedDeclaration() && scope->GetFunc() == funcInfo)
    {
        // Ensure this symbol has a slot if it needs one.
        if (sym->IsInSlot(funcInfo))
        {
            Js::PropertyId slot = sym->EnsureScopeSlot(funcInfo);
            funcInfo->FindOrAddSlotProfileId(scope, slot);
        }

        EmitUseBeforeDeclarationRuntimeError(this, lhsLocation);
    }
    else if (sym == nullptr || sym->GetIsGlobal())
    {
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();
        if (this->flags & fscrEval)
        {
            if (funcInfo->byteCodeFunction->GetIsStrictMode() && funcInfo->IsGlobalFunction())
            {
                this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, funcInfo->frameDisplayRegister, Js::OpCode::ScopedLdFldForTypeOf);
            }
            else
            {
                this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, funcInfo->GetEnvRegister(), Js::OpCode::ScopedLdFldForTypeOf);
            }
        }
        else if (this->flags & (fscrImplicitThis | fscrImplicitParents))
        {
            this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, funcInfo->GetEnvRegister(), Js::OpCode::ScopedLdFldForTypeOf);
        }
        else
        {
            this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, ByteCodeGenerator::RootObjectRegister, Js::OpCode::LdRootFldForTypeOf);
        }
    }
    else if (sym->IsInSlot(funcInfo) || envIndex != -1)
    {
        // Make sure the property has a slot. This will bump up the size of the slot array if necessary.
        Js::PropertyId slot = sym->EnsureScopeSlot(funcInfo);
        Js::ProfileId profileId = funcInfo->FindOrAddSlotProfileId(scope, slot);
        Js::RegSlot tmpLocation = funcInfo->AcquireTmpRegister();
        bool chkBlockVar = NeedCheckBlockVar(sym, scope, funcInfo);
        Js::OpCode op;

        op = this->GetLdSlotOp(scope, envIndex, scopeLocation, funcInfo);
        slot = slot + (sym->GetScope()->GetIsObject() ? 0 : Js::ScopeSlots::FirstSlotIndex);

        if (envIndex != -1)
        {
            this->m_writer.SlotI2(op, tmpLocation, envIndex + Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var), slot, profileId);
        }
        else if (scopeLocation != Js::Constants::NoRegister &&
                 (scopeLocation == funcInfo->frameSlotsRegister || scopeLocation == funcInfo->frameObjRegister))
        {
            this->m_writer.SlotI1(op, tmpLocation, slot, profileId);
        }
        else if (scope->HasInnerScopeIndex())
        {
            this->m_writer.SlotI2(op, tmpLocation, scope->GetInnerScopeIndex(), slot, profileId);
        }
        else
        {
            Assert(scope->GetIsObject());
            this->m_writer.Slot(op, tmpLocation, scopeLocation, slot, profileId);
        }

        if (chkBlockVar)
        {
            this->m_writer.Reg1(Js::OpCode::ChkUndecl, tmpLocation);
        }

        this->m_writer.Reg2(Js::OpCode::Typeof, lhsLocation, tmpLocation);
        funcInfo->ReleaseTmpRegister(tmpLocation);
    }
    else
    {
        this->m_writer.Reg2(Js::OpCode::Typeof, lhsLocation, sym->GetLocation());
    }

    if (fLabelDefined)
    {
        this->m_writer.MarkLabel(doneLabel);
    }
}

void ByteCodeGenerator::EnsureNoRedeclarations(ParseNode *pnodeBlock, FuncInfo *funcInfo)
{
    // Emit dynamic runtime checks for variable re-declarations. Only necessary for global functions (script or eval).
    // In eval only var declarations can cause redeclaration, and only in non-strict mode, because let/const variables
    // remain local to the eval code.

    Assert(pnodeBlock->nop == knopBlock);
    Assert(pnodeBlock->sxBlock.blockType == PnodeBlockType::Global || pnodeBlock->sxBlock.scope->GetScopeType() == ScopeType_GlobalEvalBlock);

    if (!(this->flags & fscrEvalCode))
    {
        IterateBlockScopedVariables(pnodeBlock, [this](ParseNode *pnode)
        {
            FuncInfo *funcInfo = this->TopFuncInfo();
            Symbol *sym = pnode->sxVar.sym;

            Assert(sym->GetIsGlobal());

            Js::PropertyId propertyId = sym->EnsurePosition(this);

            this->m_writer.ElementRootU(Js::OpCode::EnsureNoRootFld, funcInfo->FindOrAddReferencedPropertyId(propertyId));
        });
    }

    for (ParseNode *pnode = funcInfo->root->sxFnc.pnodeVars; pnode; pnode = pnode->sxVar.pnodeNext)
    {
        Symbol* sym = pnode->sxVar.sym;

        if (sym == nullptr || pnode->sxVar.isBlockScopeFncDeclVar)
            continue;

        if (sym->GetIsCatch() || (pnode->nop == knopVarDecl && sym->GetIsBlockVar()))
        {
            // The init node was bound to the catch object, because it's inside a catch and has the
            // same name as the catch object. But we want to define a user var at function scope,
            // so find the right symbol. (We'll still assign the RHS value to the catch object symbol.)
            // This also applies to a var declaration in the same scope as a let declaration.

            // Assert that catch cannot be at function scope and let and var at function scope is redeclaration error.
            Assert(sym->GetIsCatch() || funcInfo->bodyScope != sym->GetScope() || !this->scriptContext->GetConfig()->IsBlockScopeEnabled());
            sym = funcInfo->bodyScope->FindLocalSymbol(sym->GetName());
            Assert(sym && !sym->GetIsCatch() && !sym->GetIsBlockVar());
        }

        Assert(sym->GetIsGlobal());

        if (sym->GetSymbolType() == STVariable)
        {
            Js::PropertyId propertyId = sym->EnsurePosition(this);

            if (this->flags & fscrEval)
            {
                if (!funcInfo->byteCodeFunction->GetIsStrictMode())
                {
                    this->m_writer.ScopedProperty(Js::OpCode::ScopedEnsureNoRedeclFld, ByteCodeGenerator::RootObjectRegister,
                                                  funcInfo->FindOrAddReferencedPropertyId(propertyId));
                }
            }
            else
            {
                this->m_writer.ElementRootU(Js::OpCode::EnsureNoRootRedeclFld, funcInfo->FindOrAddReferencedPropertyId(propertyId));
            }
        }
    }
}

void ByteCodeGenerator::RecordAllIntConstants(FuncInfo * funcInfo)
{
    Js::FunctionBody *byteCodeFunction = this->TopFuncInfo()->GetParsedFunctionBody();
    funcInfo->constantToRegister.Map([byteCodeFunction](unsigned int val, Js::RegSlot location)
    {
        byteCodeFunction->RecordIntConstant(byteCodeFunction->MapRegSlot(location), val);
    });
}

void ByteCodeGenerator::RecordAllStrConstants(FuncInfo * funcInfo)
{
    Js::FunctionBody *byteCodeFunction = this->TopFuncInfo()->GetParsedFunctionBody();
    funcInfo->stringToRegister.Map([byteCodeFunction](IdentPtr pid, Js::RegSlot location)
    {
        byteCodeFunction->RecordStrConstant(byteCodeFunction->MapRegSlot(location), pid->Psz(), pid->Cch());
    });
}

void ByteCodeGenerator::RecordAllStringTemplateCallsiteConstants(FuncInfo* funcInfo)
{
    Js::FunctionBody *byteCodeFunction = this->TopFuncInfo()->GetParsedFunctionBody();
    funcInfo->stringTemplateCallsiteRegisterMap.Map([byteCodeFunction](ParseNodePtr pnode, Js::RegSlot location)
    {
        Js::ScriptContext* scriptContext = byteCodeFunction->GetScriptContext();
        Js::JavascriptLibrary* library = scriptContext->GetLibrary();
        Js::RecyclableObject* callsiteObject = library->TryGetStringTemplateCallsiteObject(pnode);

        if (callsiteObject == nullptr)
        {
            Js::RecyclableObject* rawArray = ByteCodeGenerator::BuildArrayFromStringList(pnode->sxStrTemplate.pnodeStringRawLiterals, pnode->sxStrTemplate.countStringLiterals, scriptContext);
            rawArray->Freeze();

            callsiteObject = ByteCodeGenerator::BuildArrayFromStringList(pnode->sxStrTemplate.pnodeStringLiterals, pnode->sxStrTemplate.countStringLiterals, scriptContext);
            callsiteObject->SetPropertyWithAttributes(Js::PropertyIds::raw, rawArray, PropertyNone, nullptr);
            callsiteObject->Freeze();

            library->AddStringTemplateCallsiteObject(callsiteObject);
        }

        byteCodeFunction->RecordConstant(byteCodeFunction->MapRegSlot(location), callsiteObject);
    });
}

bool IsApplyArgs(ParseNode* callNode)
{
    ParseNode* target = callNode->sxCall.pnodeTarget;
    ParseNode* args = callNode->sxCall.pnodeArgs;
    if ((target != nullptr) && (target->nop == knopDot))
    {
        ParseNode* lhsNode = target->sxBin.pnode1;
        if ((lhsNode != nullptr) && ((lhsNode->nop == knopDot) || (lhsNode->nop == knopName)) && !IsArguments(lhsNode))
        {
            ParseNode* nameNode = target->sxBin.pnode2;
            if (nameNode != nullptr)
            {
                bool nameIsApply = nameNode->sxPid.PropertyIdFromNameNode() == Js::PropertyIds::apply;
                if (nameIsApply && args != nullptr && args->nop == knopList)
                {
                    ParseNode* arg1 = args->sxBin.pnode1;
                    ParseNode* arg2 = args->sxBin.pnode2;
                    if ((arg1 != nullptr) && (arg1->nop == knopThis) && (arg2 != nullptr) && (arg2->nop == knopName) && (arg2->sxPid.sym != nullptr))
                    {
                        return arg2->sxPid.sym->GetIsArguments();
                    }
                }
            }
        }
    }
    return false;
}

void PostCheckApplyEnclosesArgs(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, ApplyCheck* applyCheck)
{
    if ((pnode == nullptr) || (!applyCheck->matches))
    {
        return;
    }

    if (pnode->nop == knopCall)
    {
        if ((!pnode->isUsed) && IsApplyArgs(pnode))
        {
            if (!applyCheck->insideApplyCall)
            {
                applyCheck->matches = false;
            }
            applyCheck->insideApplyCall = false;
        }
    }
}

void CheckApplyEnclosesArgs(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, ApplyCheck* applyCheck)
{
    if ((pnode == nullptr) || (!applyCheck->matches))
    {
        return;
    }

    switch (pnode->nop)
    {
    case knopName:
    {
        Symbol* sym = pnode->sxPid.sym;
        if (sym != nullptr)
        {
            if (sym->GetIsArguments())
            {
                if (!applyCheck->insideApplyCall)
                {
                    applyCheck->matches = false;
                }
            }
        }
        break;
    }

    case knopCall:
        if ((!pnode->isUsed) && IsApplyArgs(pnode))
        {
            // no nested apply calls
            if (applyCheck->insideApplyCall)
            {
                applyCheck->matches = false;
            }
            else
            {
                applyCheck->insideApplyCall = true;
                applyCheck->sawApply = true;
                pnode->sxCall.isApplyCall = true;
            }
        }
        break;
    }
}

unsigned int CountArguments(ParseNode *pnode, BOOL *pSideEffect = nullptr)
{
    // If the caller passed us a pSideEffect, it wants to know whether there are potential
    // side-effects in the argument list. We need to know this so that the call target
    // operands can be preserved if necessary.
    // For now, treat any non-leaf op as a potential side-effect. This causes no detectable slowdowns,
    // but we can be more precise if we need to be.
    if (pSideEffect)
    {
        *pSideEffect = FALSE;
    }

    unsigned int argCount = 1;
    if (pnode != nullptr)
    {
        while (pnode->nop == knopList)
        {
            argCount++;
            if (pSideEffect && !(ParseNode::Grfnop(pnode->sxBin.pnode1->nop) & fnopLeaf))
            {
                *pSideEffect = TRUE;
            }
            pnode = pnode->sxBin.pnode2;
        }
        argCount++;
        if (pSideEffect && !(ParseNode::Grfnop(pnode->nop) & fnopLeaf))
        {
            *pSideEffect = TRUE;
        }
    }

    return argCount;
}

void SaveOpndValue(ParseNode *pnode, FuncInfo *funcInfo)
{
    // Save a local name to a register other than its home location.
    // This guards against side-effects in cases like x.foo(x = bar()).
    Symbol *sym = nullptr;
    if (pnode->nop == knopName)
    {
        sym = pnode->sxPid.sym;
    }
    else if (pnode->nop == knopComputedName)
    {
        ParseNode *pnode1 = pnode->sxUni.pnode1;
        if (pnode1->nop == knopName)
        {
            sym = pnode1->sxPid.sym;
        }
    }

    if (sym == nullptr)
    {
        return;
    }

    // If the target is a local being kept in its home location,
    // protect the target's value in the event the home location is overwritten.
    if (pnode->location != Js::Constants::NoRegister &&
        sym->GetScope()->GetFunc() == funcInfo &&
        pnode->location == sym->GetLocation())
    {
        pnode->location = funcInfo->AcquireTmpRegister();
    }
}

void ByteCodeGenerator::StartStatement(ParseNode* node)
{
    Assert(TopFuncInfo() != nullptr);
    m_writer.StartStatement(node, TopFuncInfo()->curTmpReg - TopFuncInfo()->firstTmpReg);
}

void ByteCodeGenerator::EndStatement(ParseNode* node)
{
    m_writer.EndStatement(node);
}

void ByteCodeGenerator::StartSubexpression(ParseNode* node)
{
    Assert(TopFuncInfo() != nullptr);
    m_writer.StartSubexpression(node);
}

void ByteCodeGenerator::EndSubexpression(ParseNode* node)
{
    m_writer.EndSubexpression(node);
}

void EmitReference(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    // Generate code for the LHS of an assignment.
    switch (pnode->nop)
    {
    case knopDot:
        Emit(pnode->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
        break;

    case knopIndex:
        Emit(pnode->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
        Emit(pnode->sxBin.pnode2, byteCodeGenerator, funcInfo, false);
        break;

    case knopName:
        break;

    case knopArrayPattern:
    case knopObjectPattern:
        break;

    case knopCall:
    case knopNew:
        // Emit the operands of a call that will be used as a LHS.
        // These have to be emitted before the RHS, but they have to persist until
        // the end of the expression.
        // Emit the call target operands first.
        switch (pnode->sxCall.pnodeTarget->nop)
        {
        case knopDot:
        case knopIndex:
            funcInfo->AcquireLoc(pnode->sxCall.pnodeTarget);
            EmitReference(pnode->sxCall.pnodeTarget, byteCodeGenerator, funcInfo);
            break;

        case knopName:
        {
            Symbol *sym = pnode->sxCall.pnodeTarget->sxPid.sym;
            if (!sym || sym->GetLocation() == Js::Constants::NoRegister)
            {
                funcInfo->AcquireLoc(pnode->sxCall.pnodeTarget);
            }
            if (sym && (sym->IsInSlot(funcInfo) || sym->GetScope()->GetFunc() != funcInfo))
            {
                // Can't get the value from the assigned register, so load it here.
                EmitLoad(pnode->sxCall.pnodeTarget, byteCodeGenerator, funcInfo);
            }
            else
            {
                EmitReference(pnode->sxCall.pnodeTarget, byteCodeGenerator, funcInfo);
            }
            break;
        }
        default:
            EmitLoad(pnode->sxCall.pnodeTarget, byteCodeGenerator, funcInfo);
            break;
        }

        // Now the arg list. We evaluate everything now and emit the ArgOut's later.
        if (pnode->sxCall.pnodeArgs)
        {
            ParseNode *pnodeArg = pnode->sxCall.pnodeArgs;
            while (pnodeArg->nop == knopList)
            {
                Emit(pnodeArg->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
                pnodeArg = pnodeArg->sxBin.pnode2;
            }
            Emit(pnodeArg, byteCodeGenerator, funcInfo, false);
        }
        break;

    default:
        Emit(pnode, byteCodeGenerator, funcInfo, false);
        break;
    }
}

void EmitGetIterator(Js::RegSlot iteratorLocation, Js::RegSlot iterableLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo);
void EmitIteratorNext(Js::RegSlot itemLocation, Js::RegSlot iteratorLocation, Js::RegSlot nextInputLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo);
void EmitIteratorComplete(Js::RegSlot doneLocation, Js::RegSlot iteratorResultLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo);
void EmitIteratorValue(Js::RegSlot valueLocation, Js::RegSlot iteratorResultLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo);

void EmitDestructuredElement(ParseNode *elem, Js::RegSlot sourceLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo *funcInfo)
{
    switch (elem->nop)
    {
    case knopVarDecl:
    case knopLetDecl:
    case knopConstDecl:
        // We manually need to set NeedDeclaration since the node won't be visited.
        elem->sxVar.sym->SetNeedDeclaration(false);
        break;

    case knopName:
        if (elem->sxPid.sym != nullptr && elem->sxPid.sym->GetNeedDeclaration())
        {
            bool needToRelease = elem->location == Js::Constants::NoRegister;
            EmitUseBeforeDeclaration(elem, byteCodeGenerator, funcInfo);
            if (needToRelease && elem->location != Js::Constants::NoRegister)
            {
                // We have acquired register as a part of EmitUseBeforeDeclaration. We need to release that.
                funcInfo->ReleaseTmpRegister(elem->location);
            }
        }
        break;

    default:
        EmitReference(elem, byteCodeGenerator, funcInfo);
    }

    EmitAssignment(nullptr, elem, sourceLocation, byteCodeGenerator, funcInfo);
    funcInfo->ReleaseReference(elem);
}

void EmitDestructuredRestArray(ParseNode *elem, Js::RegSlot iteratorLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    Js::RegSlot restArrayLocation = funcInfo->AcquireTmpRegister();
    byteCodeGenerator->Writer()->Reg1Unsigned1(
        Js::OpCode::NewScArray,
        restArrayLocation,
        ByteCodeGenerator::DefaultArraySize);

    // BytecodeGen can't convey to IRBuilder that some of the temporaries used here are live. When we
    // have a rest parameter, a counter is used in a loop for the array index, but there is no way to
    // convey this is live on the back edge.
    // As a workaround, we have a persistent var reg that is used for the loop counter
    Js::RegSlot counterLocation = elem->location;
    // TODO[ianhall]: Is calling EnregisterConstant() during Emit phase allowed?
    Js::RegSlot zeroConstantReg = byteCodeGenerator->EnregisterConstant(0);
    byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, counterLocation, zeroConstantReg);

    // loopTop:
    Js::ByteCodeLabel loopTop = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->MarkLabel(loopTop);

    Js::RegSlot itemLocation = funcInfo->AcquireTmpRegister();

    EmitIteratorNext(itemLocation, iteratorLocation, Js::Constants::NoRegister, byteCodeGenerator, funcInfo);

    Js::RegSlot doneLocation = funcInfo->AcquireTmpRegister();
    EmitIteratorComplete(doneLocation, itemLocation, byteCodeGenerator, funcInfo);

    Js::ByteCodeLabel iteratorDone = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, iteratorDone, doneLocation);

    Js::RegSlot valueLocation = funcInfo->AcquireTmpRegister();
    EmitIteratorValue(valueLocation, itemLocation, byteCodeGenerator, funcInfo);

    byteCodeGenerator->Writer()->Element(
        ByteCodeGenerator::GetStElemIOpCode(funcInfo),
        valueLocation, restArrayLocation, counterLocation);
    funcInfo->ReleaseTmpRegister(valueLocation);
    funcInfo->ReleaseTmpRegister(doneLocation);
    funcInfo->ReleaseTmpRegister(itemLocation);

    byteCodeGenerator->Writer()->Reg2(Js::OpCode::Incr_A, counterLocation, counterLocation);
    byteCodeGenerator->Writer()->Br(loopTop);

    // iteratorDone:
    byteCodeGenerator->Writer()->MarkLabel(iteratorDone);

    ParseNode *restElem = elem->sxUni.pnode1;
    EmitDestructuredElement(restElem, restArrayLocation, byteCodeGenerator, funcInfo);

    funcInfo->ReleaseTmpRegister(restArrayLocation);
}

/*
  EmitDestructuredArray(lhsArray, rhs):
    iterator = rhs[@@iterator]

    if lhsArray empty
      return

    for each element in lhsArray except rest
      value = iterator.next()
      if element is a nested destructured array
        EmitDestructuredArray(element, value)
      else
        if value is undefined and there is an initializer
          evaluate initializer
          evaluate element reference
          element = initializer
        else
          element = value

    if lhsArray has a rest element
      rest = []
      while iterator is not done
        value = iterator.next()
        rest.append(value)
*/
void EmitDestructuredArray(
    ParseNode *lhs,
    Js::RegSlot rhsLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{
    byteCodeGenerator->StartStatement(lhs);
    Js::RegSlot iteratorLocation = funcInfo->AcquireTmpRegister();

    EmitGetIterator(iteratorLocation, rhsLocation, byteCodeGenerator, funcInfo);

    Assert(lhs->nop == knopArrayPattern);
    ParseNode *list = lhs->sxArrLit.pnode1;

    if (list == nullptr)
    {
        // No elements to bind or assign.
        funcInfo->ReleaseTmpRegister(iteratorLocation);
        return;
    }

    ParseNode *elem = nullptr;
    while (list != nullptr)
    {
        ParseNode *init = nullptr;

        if (list->nop == knopList)
        {
            elem = list->sxBin.pnode1;
        }
        else
        {
            elem = list;
        }

        if (elem->nop == knopEllipsis)
        {
            // Rest must be the last argument - no need to continue.
            break;
        }

        switch (elem->nop)
        {
        case knopAsg:
            // An assignment node will always have an initializer
            init = elem->sxBin.pnode2;
            elem = elem->sxBin.pnode1;
            break;

        case knopVarDecl:
        case knopLetDecl:
        case knopConstDecl:
            init = elem->sxVar.pnodeInit;
            break;

        default:
            break;
        }

        byteCodeGenerator->StartStatement(elem);

        Js::RegSlot itemLocation = funcInfo->AcquireTmpRegister();

        EmitIteratorNext(itemLocation, iteratorLocation, Js::Constants::NoRegister, byteCodeGenerator, funcInfo);

        if (elem->nop == knopEmpty)
        {
            // Missing elements only require a next call.
            funcInfo->ReleaseTmpRegister(itemLocation);
            if (list->nop == knopList)
            {
                list = list->sxBin.pnode2;
                continue;
            }
            else
            {
                break;
            }
        }

        Js::RegSlot doneLocation = funcInfo->AcquireTmpRegister();
        EmitIteratorComplete(doneLocation, itemLocation, byteCodeGenerator, funcInfo);

        // If the iterator hasn't completed, skip assigning undefined.
        Js::ByteCodeLabel iteratorAlreadyDone = byteCodeGenerator->Writer()->DefineLabel();
        byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, iteratorAlreadyDone, doneLocation);
        funcInfo->ReleaseTmpRegister(doneLocation);

        // We're not done with the iterator, so assign the .next() value.
        Js::RegSlot valueLocation = funcInfo->AcquireTmpRegister();
        EmitIteratorValue(valueLocation, itemLocation, byteCodeGenerator, funcInfo);
        Js::ByteCodeLabel beforeDefaultAssign = byteCodeGenerator->Writer()->DefineLabel();
        byteCodeGenerator->Writer()->Br(beforeDefaultAssign);

        // iteratorAlreadyDone:
        byteCodeGenerator->Writer()->MarkLabel(iteratorAlreadyDone);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, valueLocation, funcInfo->undefinedConstantRegister);

        // beforeDefaultAssign:
        byteCodeGenerator->Writer()->MarkLabel(beforeDefaultAssign);

        if (elem->IsPattern())
        {
            // If we get an undefined value and have an initializer, use it in place of undefined.
            if (init != nullptr)
            {
                /*
                the IR builder uses two symbols for a temp register in the if else path
                R9 <- R3
                if (...)
                R9 <- R2
                R10 = R9.<property>  // error -> IR creates a new lifetime for the if path, and the direct path dest is not referenced
                hence we have to create a new temp

                TEMP REG USED TO FIX THIS PRODUCES THIS
                R9 <- R3
                if (BrEq_A R9, R3)
                R10 <- R2               :
                else
                R10 <- R9               : skipdefault
                ...  = R10[@@iterator]  : loadIter
                */

                // Temp Register
                Js::RegSlot valueLocationTmp = funcInfo->AcquireTmpRegister();
                byteCodeGenerator->StartStatement(init);

                Js::ByteCodeLabel skipDefault = byteCodeGenerator->Writer()->DefineLabel();
                Js::ByteCodeLabel loadIter = byteCodeGenerator->Writer()->DefineLabel();

                // check value is undefined
                byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrSrNeq_A, skipDefault, valueLocation, funcInfo->undefinedConstantRegister);

                // Evaluate the default expression and assign it.
                Emit(init, byteCodeGenerator, funcInfo, false);
                byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, valueLocationTmp, init->location);
                funcInfo->ReleaseLoc(init);

                // jmp to loadIter
                byteCodeGenerator->Writer()->Br(loadIter);

                // skipDefault:
                byteCodeGenerator->Writer()->MarkLabel(skipDefault);
                byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, valueLocationTmp, valueLocation);

                // loadIter:
                // @@iterator
                byteCodeGenerator->Writer()->MarkLabel(loadIter);
                byteCodeGenerator->EndStatement(init);

                if (elem->nop == knopObjectPattern)
                {
                    EmitDestructuredObject(elem, valueLocationTmp, byteCodeGenerator, funcInfo);
                }
                else
                {
                    // Recursively emit a destructured array using the current .next() as the RHS.
                    EmitDestructuredArray(elem, valueLocationTmp, byteCodeGenerator, funcInfo);
                }

                funcInfo->ReleaseTmpRegister(valueLocationTmp);
            }
            else
            {
                if (elem->nop == knopObjectPattern)
                {
                    EmitDestructuredObject(elem, valueLocation, byteCodeGenerator, funcInfo);
                }
                else
                {
                    // Recursively emit a destructured array using the current .next() as the RHS.
                    EmitDestructuredArray(elem, valueLocation, byteCodeGenerator, funcInfo);
                }
            }
        }
        else
        {
            EmitDestructuredValueOrInitializer(elem, valueLocation, init, byteCodeGenerator, funcInfo);
        }

        funcInfo->ReleaseTmpRegister(valueLocation);
        funcInfo->ReleaseTmpRegister(itemLocation);

        byteCodeGenerator->EndStatement(elem);

        if (list->nop == knopList)
        {
            list = list->sxBin.pnode2;
        }
        else
        {
            break;
        }
    }

    // If we saw a rest element, emit the rest array.
    if (elem != nullptr && elem->nop == knopEllipsis)
    {
        EmitDestructuredRestArray(elem, iteratorLocation, byteCodeGenerator, funcInfo);
    }
    funcInfo->ReleaseTmpRegister(iteratorLocation);

    byteCodeGenerator->EndStatement(lhs);
}

void EmitNameInvoke(Js::RegSlot lhsLocation,
    Js::RegSlot objectLocation,
    ParseNodePtr nameNode,
    ByteCodeGenerator* byteCodeGenerator,
    FuncInfo* funcInfo)
{
    Assert(nameNode != nullptr);
    if (nameNode->nop == knopComputedName)
    {
        ParseNodePtr pnode1 = nameNode->sxUni.pnode1;
        Emit(pnode1, byteCodeGenerator, funcInfo, false/*isConstructorCall*/);

        byteCodeGenerator->Writer()->Element(Js::OpCode::LdElemI_A, lhsLocation, objectLocation, pnode1->location);
        funcInfo->ReleaseLoc(pnode1);
    }
    else
    {
        Assert(nameNode->nop == knopName || nameNode->nop == knopStr);
        Symbol *sym = nameNode->sxPid.sym;
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(byteCodeGenerator) : nameNode->sxPid.pid->GetPropertyId();

        uint cacheId = funcInfo->FindOrAddInlineCacheId(objectLocation, propertyId, false/*isLoadMethod*/, false/*isStore*/);
        byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, lhsLocation, objectLocation, cacheId);
    }
}

void EmitDestructuredValueOrInitializer(ParseNodePtr lhsElementNode,
    Js::RegSlot rhsLocation,
    ParseNodePtr initializer,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{
    // If we have initializer we need to see if the destructured value is undefined or not - if it is undefined we need to assign initializer

    Js::ByteCodeLabel useDefault = -1;
    Js::ByteCodeLabel end = -1;
    Js::RegSlot rhsLocationTmp = rhsLocation;

    if (initializer != nullptr)
    {
        rhsLocationTmp = funcInfo->AcquireTmpRegister();

        useDefault = byteCodeGenerator->Writer()->DefineLabel();
        end = byteCodeGenerator->Writer()->DefineLabel();

        byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrSrEq_A, useDefault, rhsLocation, funcInfo->undefinedConstantRegister);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, rhsLocationTmp, rhsLocation);

        byteCodeGenerator->Writer()->Br(end);
        byteCodeGenerator->Writer()->MarkLabel(useDefault);

        Emit(initializer, byteCodeGenerator, funcInfo, false/*isContructorCall*/);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, rhsLocationTmp, initializer->location);
        funcInfo->ReleaseLoc(initializer);

        byteCodeGenerator->Writer()->MarkLabel(end);
    }

    if (lhsElementNode->nop == knopArrayPattern)
    {
        EmitDestructuredArray(lhsElementNode, rhsLocationTmp, byteCodeGenerator, funcInfo);
    }
    else if (lhsElementNode->nop == knopObjectPattern)
    {
        EmitDestructuredObject(lhsElementNode, rhsLocationTmp, byteCodeGenerator, funcInfo);
    }
    else
    {
        EmitDestructuredElement(lhsElementNode, rhsLocationTmp, byteCodeGenerator, funcInfo);
    }

    if (initializer != nullptr)
    {
        funcInfo->ReleaseTmpRegister(rhsLocationTmp);
    }
}

void EmitDestructuredObjectMember(ParseNodePtr memberNode,
    Js::RegSlot rhsLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{
    Assert(memberNode->nop == knopObjectPatternMember);

    Js::RegSlot nameLocation = funcInfo->AcquireTmpRegister();
    EmitNameInvoke(nameLocation, rhsLocation, memberNode->sxBin.pnode1, byteCodeGenerator, funcInfo);

    // Imagine we are transforming
    // {x:x1} = {} to x1 = {}.x  (here x1 is the second node of the member but that is our lhsnode)

    ParseNodePtr lhsElementNode = memberNode->sxBin.pnode2;
    ParseNodePtr init = nullptr;
    if (lhsElementNode->IsVarLetOrConst())
    {
        init = lhsElementNode->sxVar.pnodeInit;
    }
    else if (lhsElementNode->nop == knopAsg)
    {
        init = lhsElementNode->sxBin.pnode2;
        lhsElementNode = lhsElementNode->sxBin.pnode1;
    }

    EmitDestructuredValueOrInitializer(lhsElementNode, nameLocation, init, byteCodeGenerator, funcInfo);

    funcInfo->ReleaseTmpRegister(nameLocation);
}

void EmitDestructuredObject(ParseNode *lhs,
    Js::RegSlot rhsLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{
    Assert(lhs->nop == knopObjectPattern);
    ParseNodePtr pnode1 = lhs->sxUni.pnode1;
    if (pnode1 != nullptr)
    {
        Assert(pnode1->nop == knopList || pnode1->nop == knopObjectPatternMember);

        byteCodeGenerator->StartStatement(lhs);

        ParseNodePtr current = pnode1;
        while (current->nop == knopList)
        {
            ParseNodePtr memberNode = current->sxBin.pnode1;
            EmitDestructuredObjectMember(memberNode, rhsLocation, byteCodeGenerator, funcInfo);
            current = current->sxBin.pnode2;
        }
        EmitDestructuredObjectMember(current, rhsLocation, byteCodeGenerator, funcInfo);

        byteCodeGenerator->EndStatement(lhs);
    }
}

void EmitAssignment(
    ParseNode *asgnNode,
    ParseNode *lhs,
    Js::RegSlot rhsLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{
    switch (lhs->nop)
    {
    // assignment to a local or global variable
    case knopVarDecl:
    case knopLetDecl:
    case knopConstDecl:
    {
        Symbol *sym = lhs->sxVar.sym;
        Assert(sym != nullptr);
        byteCodeGenerator->EmitPropStore(rhsLocation, sym, nullptr, funcInfo, lhs->nop == knopLetDecl, lhs->nop == knopConstDecl);
        break;
    }

    case knopName:
    {
        byteCodeGenerator->EmitPropStore(rhsLocation, lhs->sxPid.sym, lhs->sxPid.pid, funcInfo);
        break;
    }

    // x.y =
    case knopDot:
    {
        // PutValue(x, "y", rhs)
        Js::PropertyId propertyId = lhs->sxBin.pnode2->sxPid.PropertyIdFromNameNode();

        uint cacheId = funcInfo->FindOrAddInlineCacheId(lhs->sxBin.pnode1->location, propertyId, false, true);
        if (lhs->sxBin.pnode1->nop == knopSuper)
        {
            byteCodeGenerator->Writer()->PatchablePropertyWithThisPtr(Js::OpCode::StSuperFld, rhsLocation, lhs->sxBin.pnode1->location, funcInfo->thisPointerRegister, cacheId);
        }
        else
        {
            byteCodeGenerator->Writer()->PatchableProperty(
                ByteCodeGenerator::GetStFldOpCode(funcInfo, false, false, false, false), rhsLocation, lhs->sxBin.pnode1->location, cacheId);
        }

        break;
    }

    case knopIndex:
    {
        byteCodeGenerator->Writer()->Element(
            ByteCodeGenerator::GetStElemIOpCode(funcInfo),
            rhsLocation, lhs->sxBin.pnode1->location, lhs->sxBin.pnode2->location);
        break;
    }

    case knopObjectPattern:
    {
        Assert(byteCodeGenerator->IsES6DestructuringEnabled());
        // Copy the rhs value to be the result of the assignment if needed.
        if (asgnNode != nullptr)
        {
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, asgnNode->location, rhsLocation);
        }
        return EmitDestructuredObject(lhs, rhsLocation, byteCodeGenerator, funcInfo);
    }

    case knopArrayPattern:
    {
        Assert(byteCodeGenerator->IsES6DestructuringEnabled());
        // Copy the rhs value to be the result of the assignment if needed.
        if (asgnNode != nullptr)
        {
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, asgnNode->location, rhsLocation);
        }
        return EmitDestructuredArray(lhs, rhsLocation, byteCodeGenerator, funcInfo);
    }

    case knopArray:
    case knopObject:
        // Assignment to array/object can get through to byte code gen when the parser fails to convert destructuring
        // assignment to pattern (because of structural mismatch between LHS & RHS?). Revisit when we nail
        // down early vs. runtime errors for destructuring.
        byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_CantAssignTo));
        break;

    default:
        Assert(!PHASE_ON1(Js::EarlyReferenceErrorsPhase));
        byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_CantAssignTo));
        break;
    }

    if (asgnNode != nullptr)
    {
        // We leave it up to the caller to pass this node only if the assignment expression is used.
        if (asgnNode->location != rhsLocation)
        {
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, asgnNode->location, rhsLocation);
        }
    }
}

void EmitLoad(
    ParseNode *lhs,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{
    // Emit the instructions to load the value into the LHS location. Do not assign/free any temps
    // in the process.
    // We usually get here as part of an op-equiv expression: x.y += z;
    // In such a case, x has to be emitted first, then the value of x.y loaded (by this function), then z emitted.
    switch (lhs->nop)
    {

    // load of a local or global variable
    case knopName:
    {
        funcInfo->AcquireLoc(lhs);
        byteCodeGenerator->EmitPropLoad(lhs->location, lhs->sxPid.sym, lhs->sxPid.pid, funcInfo);
        break;
    }

    // = x.y
    case knopDot:
    {
        // get field id for "y"
        Js::PropertyId propertyId = lhs->sxBin.pnode2->sxPid.PropertyIdFromNameNode();
        funcInfo->AcquireLoc(lhs);
        EmitReference(lhs, byteCodeGenerator, funcInfo);
        uint cacheId = funcInfo->FindOrAddInlineCacheId(lhs->sxBin.pnode1->location, propertyId, false, false);
        byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, lhs->location, lhs->sxBin.pnode1->location, cacheId);
        break;
    }

    case knopIndex:
        funcInfo->AcquireLoc(lhs);
        EmitReference(lhs, byteCodeGenerator, funcInfo);
        byteCodeGenerator->Writer()->Element(
            Js::OpCode::LdElemI_A, lhs->location, lhs->sxBin.pnode1->location, lhs->sxBin.pnode2->location);
        break;

    // f(x) +=
    case knopCall:
        funcInfo->AcquireLoc(lhs);
        EmitReference(lhs, byteCodeGenerator, funcInfo);
        EmitCall(lhs, /*rhs=*/ Js::Constants::NoRegister, byteCodeGenerator, funcInfo, /*fReturnValue=*/ false, /*fAssignRegs=*/ false, /*fHasNewTarget=*/ false);
        break;

    default:
        funcInfo->AcquireLoc(lhs);
        Emit(lhs, byteCodeGenerator, funcInfo, false);
        break;
    }
}

void EmitList(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    if (pnode != nullptr)
    {
        while (pnode->nop == knopList)
        {
            byteCodeGenerator->EmitTopLevelStatement(pnode->sxBin.pnode1, funcInfo, false);
            pnode = pnode->sxBin.pnode2;
        }
        byteCodeGenerator->EmitTopLevelStatement(pnode, funcInfo, false);
    }
}

void EmitSpreadArgToListBytecodeInstr(ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, Js::RegSlot argLoc, Js::ProfileId callSiteId, Js::ArgSlot &argIndex)
{
    Js::RegSlot regVal = funcInfo->AcquireTmpRegister();
    byteCodeGenerator->Writer()->Reg2(Js::OpCode::LdCustomSpreadIteratorList, regVal, argLoc);
    byteCodeGenerator->Writer()->ArgOut<true>(++argIndex, regVal, callSiteId);
    funcInfo->ReleaseTmpRegister(regVal);
}

size_t EmitArgs(
    ParseNode *pnode,
    BOOL fAssignRegs,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    Js::ProfileId callSiteId,
    Js::AuxArray<uint32> *spreadIndices = nullptr
    )
{
    Js::ArgSlot argIndex = 0;
    Js::ArgSlot spreadIndex = 0;

    if (pnode != nullptr)
    {
        while (pnode->nop == knopList)
        {
            // If this is a put, the arguments have already been evaluated (see EmitReference).
            // We just need to emit the ArgOut instructions.
            if (fAssignRegs)
            {
                Emit(pnode->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
            }

            if (pnode->sxBin.pnode1->nop == knopEllipsis)
            {
                Assert(spreadIndices != nullptr);
                spreadIndices->elements[spreadIndex++] = argIndex + 1; // account for 'this'
                EmitSpreadArgToListBytecodeInstr(byteCodeGenerator, funcInfo, pnode->sxBin.pnode1->location, callSiteId, argIndex);
            }
            else
            {
                byteCodeGenerator->Writer()->ArgOut<true>(++argIndex, pnode->sxBin.pnode1->location, callSiteId);
            }
            if (fAssignRegs)
            {
                funcInfo->ReleaseLoc(pnode->sxBin.pnode1);
            }

            pnode = pnode->sxBin.pnode2;
        }

        // If this is a put, the call target has already been evaluated (see EmitReference).
        if (fAssignRegs)
        {
            Emit(pnode, byteCodeGenerator, funcInfo, false);
        }

        if (pnode->nop == knopEllipsis)
        {
            Assert(spreadIndices != nullptr);
            spreadIndices->elements[spreadIndex++] = argIndex + 1; // account for 'this'
            EmitSpreadArgToListBytecodeInstr(byteCodeGenerator, funcInfo, pnode->location, callSiteId, argIndex);
        }
        else
        {
            byteCodeGenerator->Writer()->ArgOut<true>(++argIndex, pnode->location, callSiteId);
        }

        if (fAssignRegs)
        {
            funcInfo->ReleaseLoc(pnode);
        }
    }

    return argIndex;
}

void EmitArgListStart(
    Js::RegSlot thisLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    Js::ProfileId callSiteId)
{
    if (thisLocation != Js::Constants::NoRegister)
    {
        // Emit the "this" object.
        byteCodeGenerator->Writer()->ArgOut<true>(0, thisLocation, callSiteId);
    }
}

Js::ArgSlot EmitArgListEnd(
    ParseNode *pnode,
    Js::RegSlot rhsLocation,
    Js::RegSlot thisLocation,
    Js::RegSlot evalLocation,
    Js::RegSlot newTargetLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    size_t argIndex,
    Js::ProfileId callSiteId)
{
    BOOL fEvalInModule = false;
    BOOL fIsPut = (rhsLocation != Js::Constants::NoRegister);
    BOOL fIsEval = (evalLocation != Js::Constants::NoRegister);
    BOOL fHasNewTarget = (newTargetLocation != Js::Constants::NoRegister);

    Js::ArgSlot argSlotIndex = (Js::ArgSlot) argIndex;
    static const Js::ArgSlot maxExtraArgSlot = 4;  // max(extraEvalArg, extraArg), where extraEvalArg==2 (moduleRoot,env), extraArg==4 (this, eval, evalInModule, newTarget)

    // check for integer overflow with margin for increments below to calculate argument count
    if ((size_t)argSlotIndex != argIndex || argSlotIndex + maxExtraArgSlot < argSlotIndex)
    {
        Js::Throw::OutOfMemory();
    }

    Js::ArgSlot evalIndex;

    if (fIsPut)
    {
        // Emit the assigned value as an additional operand. Note that the value has already been evaluated.
        // We just need to emit the ArgOut instruction.
        argSlotIndex++;
        byteCodeGenerator->Writer()->ArgOut<true>(argSlotIndex, rhsLocation, callSiteId);
    }

    if (fIsEval && argSlotIndex > 0)
    {
        Assert(!fHasNewTarget);

        // Pass the frame display as an extra argument to "eval".
        // Do this only if eval is called with some args
        Js::RegSlot evalEnv;
        if (funcInfo->IsGlobalFunction() && !(funcInfo->GetIsStrictMode() && byteCodeGenerator->GetFlags() & fscrEval))
        {
            // Use current environment as the environment for the function being called when:
            // - this is the root global function (not an eval's global function)
            // - this is an eval's global function that is not in strict mode (see else block)
            evalEnv = funcInfo->GetEnvRegister();
        }
        else
        {
            // Use the frame display as the environment for the function being called when:
            // - this is not a global function and thus it will have its own scope
            // - this is an eval's global function that is in strict mode, since in strict mode the eval's global function
            //   has its own scope
            evalEnv = funcInfo->frameDisplayRegister;
        }

        evalEnv = byteCodeGenerator->PrependLocalScopes(evalEnv, evalLocation, funcInfo);

        Js::ModuleID moduleID = byteCodeGenerator->GetModuleID();
        if (moduleID != kmodGlobal)
        {
            // Pass both the module root and the environment.
            fEvalInModule = true;
            byteCodeGenerator->Writer()->ArgOut<true>(argSlotIndex + 1, ByteCodeGenerator::RootObjectRegister, callSiteId);
            evalIndex = argSlotIndex + 2;
        }
        else
        {
            // Just pass the environment.
            evalIndex = argSlotIndex + 1;
        }

        if (evalEnv == funcInfo->GetEnvRegister() || evalEnv == funcInfo->frameDisplayRegister)
        {
            byteCodeGenerator->Writer()->ArgOutEnv(evalIndex);
        }
        else
        {
            byteCodeGenerator->Writer()->ArgOut<false>(evalIndex, evalEnv, callSiteId);
        }
    }

    if (fHasNewTarget)
    {
        Assert(!fIsEval);

        byteCodeGenerator->Writer()->ArgOut<true>(argSlotIndex + 1, newTargetLocation, callSiteId);
    }

    Js::ArgSlot argIntCount = argSlotIndex + 1 + (Js::ArgSlot)fIsEval + (Js::ArgSlot)fEvalInModule + (Js::ArgSlot)fHasNewTarget;

    // eval and no args passed, return 1 as argument count
    if (fIsEval && pnode == nullptr)
    {
        return 1;
    }

    return argIntCount;
}

Js::ArgSlot EmitArgList(
    ParseNode *pnode,
    Js::RegSlot rhsLocation,
    Js::RegSlot thisLocation,
    Js::RegSlot newTargetLocation,
    BOOL fIsEval,
    BOOL fAssignRegs,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    Js::ProfileId callSiteId,
    uint16 spreadArgCount = 0,
    Js::AuxArray<uint32> **spreadIndices = nullptr)
{
    // This function emits the arguments for a call.
    // ArgOut's with uses immediately following defs.

    EmitArgListStart(thisLocation, byteCodeGenerator, funcInfo, callSiteId);

    Js::RegSlot evalLocation = Js::Constants::NoRegister;

    //
    // If Emitting arguments for eval and assigning registers, get a tmpLocation for eval.
    // This would be used while generating frameDisplay in EmitArgListEnd.
    //
    if (fIsEval)
    {
        evalLocation = funcInfo->AcquireTmpRegister();
    }

    if (spreadArgCount > 0)
    {
        const size_t extraAlloc = spreadArgCount * sizeof(uint32);
        Assert(spreadIndices != nullptr);
        *spreadIndices = AnewPlus(byteCodeGenerator->GetAllocator(), extraAlloc, Js::AuxArray<uint32>, spreadArgCount);
    }

    size_t argIndex = EmitArgs(pnode, fAssignRegs, byteCodeGenerator, funcInfo, callSiteId, spreadIndices == nullptr ? nullptr : *spreadIndices);

    Js::ArgSlot argumentsCount = EmitArgListEnd(pnode, rhsLocation, thisLocation, evalLocation, newTargetLocation, byteCodeGenerator, funcInfo, argIndex, callSiteId);

    if (fIsEval)
    {
        funcInfo->ReleaseTmpRegister(evalLocation);
    }

    return argumentsCount;
}

void EmitConstantArgsToVarArray(ByteCodeGenerator *byteCodeGenerator, __out_ecount(argCount) Js::Var *vars, ParseNode *args, uint argCount)
{
    uint index = 0;
    while (args->nop == knopList && index < argCount)
    {
        if (args->sxBin.pnode1->nop == knopInt)
        {
            int value = args->sxBin.pnode1->sxInt.lw;
            vars[index++] = Js::TaggedInt::ToVarUnchecked(value);
        }
        else if (args->sxBin.pnode1->nop == knopFlt)
        {
            Js::Var number = Js::JavascriptNumber::New(args->sxBin.pnode1->sxFlt.dbl, byteCodeGenerator->GetScriptContext());
#if ! FLOATVAR
            byteCodeGenerator->GetScriptContext()->BindReference(number);
#endif
            vars[index++] = number;
        }
        else
        {
            AnalysisAssert(false);
        }
        args = args->sxBin.pnode2;
    }

    if (index == argCount)
    {
        Assert(false);
        Js::Throw::InternalError();
        return;
    }

    if (args->nop == knopInt)
    {
        int value = args->sxInt.lw;
        vars[index++] = Js::TaggedInt::ToVarUnchecked(value);
    }
    else if (args->nop == knopFlt)
    {
        Js::Var number = Js::JavascriptNumber::New(args->sxFlt.dbl, byteCodeGenerator->GetScriptContext());
#if ! FLOATVAR
        byteCodeGenerator->GetScriptContext()->BindReference(number);
#endif
        vars[index++] = number;
    }
    else
    {
        AnalysisAssert(false);
    }
}

void EmitConstantArgsToIntArray(ByteCodeGenerator *byteCodeGenerator, __out_ecount(argCount) int32 *vars, ParseNode *args, uint argCount)
{
    uint index = 0;
    while (args->nop == knopList && index < argCount)
    {
        Assert(args->sxBin.pnode1->nop == knopInt);
        vars[index++] = args->sxBin.pnode1->sxInt.lw;
        args = args->sxBin.pnode2;
    }

    if (index == argCount)
    {
        Assert(false);
        Js::Throw::InternalError();
        return;
    }

    Assert(args->nop == knopInt);
    vars[index++] = args->sxInt.lw;

    Assert(index == argCount);
}

void EmitConstantArgsToFltArray(ByteCodeGenerator *byteCodeGenerator, __out_ecount(argCount) double *vars, ParseNode *args, uint argCount)
{
    uint index = 0;
    while (args->nop == knopList && index < argCount)
    {
        OpCode nop = args->sxBin.pnode1->nop;
        if (nop == knopInt)
        {
            vars[index++] = (double)args->sxBin.pnode1->sxInt.lw;
        }
        else
        {
            Assert(nop == knopFlt);
            vars[index++] = args->sxBin.pnode1->sxFlt.dbl;
        }
        args = args->sxBin.pnode2;
    }

    if (index == argCount)
    {
        Assert(false);
        Js::Throw::InternalError();
        return;
    }

    if (args->nop == knopInt)
    {
        vars[index++] = (double)args->sxInt.lw;
    }
    else
    {
        Assert(args->nop == knopFlt);
        vars[index++] = args->sxFlt.dbl;
    }

    Assert(index == argCount);
}

//
// Called when we have new Ctr(constant, constant...)
//
Js::ArgSlot EmitNewObjectOfConstants(
    ParseNode *pnode,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    unsigned int argCount)
{
    EmitArgListStart(Js::Constants::NoRegister, byteCodeGenerator, funcInfo, Js::Constants::NoProfileId);

    // Create the vars array
    Js::VarArrayVarCount *vars = AnewPlus(byteCodeGenerator->GetAllocator(), (argCount - 1) * sizeof(Js::Var), Js::VarArrayVarCount, Js::TaggedInt::ToVarUnchecked(argCount - 1));

    // Emit all constants to the vars array
    EmitConstantArgsToVarArray(byteCodeGenerator, vars->elements, pnode->sxCall.pnodeArgs, argCount - 1);

    // Finish the arg list
    Js::ArgSlot actualArgCount = EmitArgListEnd(
        pnode->sxCall.pnodeArgs,
        Js::Constants::NoRegister,
        Js::Constants::NoRegister,
        Js::Constants::NoRegister,
        Js::Constants::NoRegister,
        byteCodeGenerator,
        funcInfo,
        argCount - 1,
        Js::Constants::NoProfileId);

    // Make sure the cacheId to regSlot map in the ByteCodeWriter is left in a consistent state after writing NewScObject_A
    byteCodeGenerator->Writer()->RemoveEntryForRegSlotFromCacheIdMap(pnode->sxCall.pnodeTarget->location);

    // Generate the opcode with vars
    byteCodeGenerator->Writer()->AuxiliaryContext(
        Js::OpCode::NewScObject_A,
        funcInfo->AcquireLoc(pnode),
        vars,
        sizeof(Js::VarArray) + (argCount - 1) * sizeof(Js::Var),
        pnode->sxCall.pnodeTarget->location);

    AdeletePlus(byteCodeGenerator->GetAllocator(), (argCount - 1) * sizeof(Js::VarArrayVarCount), vars);

    return actualArgCount;
}

void EmitMethodFld(bool isRoot, bool isScoped, Js::RegSlot location, Js::RegSlot callObjLocation, Js::PropertyId propertyId, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, bool registerCacheIdForCall = true)
{
    Js::OpCode opcode;
    if (!isRoot)
    {
        if (callObjLocation == funcInfo->frameObjRegister)
        {
            opcode = Js::OpCode::LdLocalMethodFld;
        }
        else
        {
            opcode = Js::OpCode::LdMethodFld;
        }
    }
    else if (isScoped)
    {
        opcode = Js::OpCode::ScopedLdMethodFld;
    }
    else
    {
        opcode = Js::OpCode::LdRootMethodFld;
    }

    if (isScoped || !isRoot)
    {
        Assert(isScoped || !isRoot || callObjLocation == ByteCodeGenerator::RootObjectRegister);
        uint cacheId = funcInfo->FindOrAddInlineCacheId(callObjLocation, propertyId, true, false);
        if (callObjLocation == funcInfo->frameObjRegister)
        {
            byteCodeGenerator->Writer()->ElementP(opcode, location, cacheId, false /*isCtor*/, registerCacheIdForCall);
        }
        else
        {
            byteCodeGenerator->Writer()->PatchableProperty(opcode, location, callObjLocation, cacheId, false /*isCtor*/, registerCacheIdForCall);
        }
    }
    else
    {
        uint cacheId = funcInfo->FindOrAddRootObjectInlineCacheId(propertyId, true, false);
        byteCodeGenerator->Writer()->PatchableRootProperty(opcode, location, cacheId, true, false, registerCacheIdForCall);
    }
}

void EmitMethodFld(ParseNode *pnode, Js::RegSlot callObjLocation, Js::PropertyId propertyId, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, bool registerCacheIdForCall = true)
{
    // Load a call target of the form x.y(). (Call target may be a plain knopName if we're getting it from
    // the global object, etc.)
    bool isRoot = pnode->nop == knopName && (pnode->sxPid.sym == nullptr || pnode->sxPid.sym->GetIsGlobal());
    bool isScoped = (byteCodeGenerator->GetFlags() & fscrEval) != 0 ||
        (isRoot && callObjLocation != ByteCodeGenerator::RootObjectRegister);

    EmitMethodFld(isRoot, isScoped, pnode->location, callObjLocation, propertyId, byteCodeGenerator, funcInfo, registerCacheIdForCall);
}

// lhs.apply(this, arguments);
void EmitApplyCall(ParseNode* pnode, Js::RegSlot rhsLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo, BOOL fReturnValue, BOOL fAssignRegs)
{
    ParseNode* applyNode = pnode->sxCall.pnodeTarget;
    ParseNode* thisNode = pnode->sxCall.pnodeArgs->sxBin.pnode1;
    Assert(applyNode->nop == knopDot);

    ParseNode* funcNode = applyNode->sxBin.pnode1;
    Js::ByteCodeLabel slowPath = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel afterSlowPath = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel argsAlreadyCreated = byteCodeGenerator->Writer()->DefineLabel();

    Assert(applyNode->nop == knopDot);

    Emit(funcNode, byteCodeGenerator, funcInfo, false);

    funcInfo->AcquireLoc(applyNode);
    Js::PropertyId propertyId = applyNode->sxBin.pnode2->sxPid.PropertyIdFromNameNode();

    // As we won't be emitting a call instruction for apply, no need to register the cacheId for apply
    // load to be associated with the call. This is also required, as in the absence of a corresponding
    // call for apply, we won't remove the entry for "apply" cacheId from
    // ByteCodeWriter::callRegToLdFldCacheIndexMap, which is contrary to our assumption that we would
    // have removed an entry from a map upon seeing its corresponding call.
    EmitMethodFld(applyNode, funcNode->location, propertyId, byteCodeGenerator, funcInfo, false /*registerCacheIdForCall*/);

    Symbol *argSym = funcInfo->GetArgumentsSymbol();
    Assert(argSym && argSym->GetIsArguments());
    Js::RegSlot argumentsLoc = argSym->GetLocation();

    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdArgumentsFromFrame, argumentsLoc);
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrNotNull_A, argsAlreadyCreated, argumentsLoc);

    // If apply is overridden, bail to slow path.
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrFncNeqApply, slowPath, applyNode->location);

    // Note: acquire and release a temp register for this stack arg pointer instead of trying to stash it
    // in funcInfo->stackArgReg. Otherwise, we'll needlessly load and store it in jitted loop bodies and
    // may crash if we try to unbox it on the store.
    Js::RegSlot stackArgReg = funcInfo->AcquireTmpRegister();
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdStackArgPtr, stackArgReg);

    Js::RegSlot argCountLocation = funcInfo->AcquireTmpRegister();

    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdArgCnt, argCountLocation);
    byteCodeGenerator->Writer()->Reg5(Js::OpCode::ApplyArgs, funcNode->location, funcNode->location, thisNode->location, stackArgReg, argCountLocation);

    funcInfo->ReleaseTmpRegister(argCountLocation);
    funcInfo->ReleaseTmpRegister(stackArgReg);
    funcInfo->ReleaseLoc(applyNode);
    funcInfo->ReleaseLoc(funcNode);

    // Clear these nodes as they are going to be used to re-generate the slow path.
    VisitClearTmpRegs(applyNode, byteCodeGenerator, funcInfo);
    VisitClearTmpRegs(funcNode, byteCodeGenerator, funcInfo);

    byteCodeGenerator->Writer()->Br(afterSlowPath);

    // slow path
    byteCodeGenerator->Writer()->MarkLabel(slowPath);
    if (funcInfo->frameObjRegister != Js::Constants::NoRegister)
    {
        byteCodeGenerator->EmitScopeObjectInit(funcInfo);
    }
    byteCodeGenerator->LoadHeapArguments(funcInfo);

    byteCodeGenerator->Writer()->MarkLabel(argsAlreadyCreated);
    EmitCall(pnode, rhsLocation, byteCodeGenerator, funcInfo, fReturnValue, fAssignRegs,/*fHasNewTarget*/false);
    byteCodeGenerator->Writer()->MarkLabel(afterSlowPath);
}

void EmitMethodElem(ParseNode *pnode, Js::RegSlot callObjLocation, Js::RegSlot indexLocation, ByteCodeGenerator *byteCodeGenerator)
{
    // Load a call target of the form x[y]().
    byteCodeGenerator->Writer()->Element(Js::OpCode::LdMethodElem, pnode->location, callObjLocation, indexLocation);
}

void EmitCallTargetNoEvalComponents(
    ParseNode *pnodeTarget,
    BOOL fSideEffectArgs,
    Js::RegSlot *thisLocation,
    Js::RegSlot *callObjLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{
    // We first get a reference to the call target, then evaluate the arguments, then
    // evaluate the call target.

    // - emit reference to target
    //    - copy instance to scratch reg if necessary.
    //    - assign this
    //    - assign instance for dynamic/global name
    // - emit args
    // - do call (CallFld/Elem/I)

    switch (pnodeTarget->nop)
    {
    case knopDot:
        *thisLocation = pnodeTarget->sxBin.pnode1->location;
        *callObjLocation = pnodeTarget->sxBin.pnode1->location;
        break;

    case knopIndex:
        *thisLocation = pnodeTarget->sxBin.pnode1->location;
        *callObjLocation = pnodeTarget->sxBin.pnode1->location;
        break;

    case knopName:
        // If the call target is a name, do some extra work to get its instance and the "this" pointer.
        byteCodeGenerator->EmitLoadInstance(pnodeTarget->sxPid.sym, pnodeTarget->sxPid.pid, thisLocation, callObjLocation, funcInfo);
        if (*thisLocation == Js::Constants::NoRegister)
        {
            *thisLocation = funcInfo->undefinedConstantRegister;
        }

        if (byteCodeGenerator->GetScriptContext()->GetConfig()->IsLetAndConstEnabled())
        {
            EmitUseBeforeDeclaration(pnodeTarget, byteCodeGenerator, funcInfo);
        }
        break;

    default:
        *thisLocation = funcInfo->undefinedConstantRegister;
        break;
    }
}

void EmitSuperMethodBegin(
    ParseNode *pnodeTarget,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{
    FuncInfo *parentFuncInfo = funcInfo;
    if (parentFuncInfo->IsLambda())
    {
        parentFuncInfo = byteCodeGenerator->FindEnclosingNonLambda();
    }

    if (pnodeTarget->sxBin.pnode1->nop == knopSuper && parentFuncInfo->IsClassConstructor() && !parentFuncInfo->IsBaseClassConstructor())
    {
        byteCodeGenerator->EmitScopeSlotLoadThis(funcInfo, funcInfo->thisPointerRegister, /*chkUndecl*/ true);
    }
}

void EmitCallTarget(
    ParseNode *pnodeTarget,
    BOOL fSideEffectArgs,
    Js::RegSlot *thisLocation,
    Js::RegSlot *callObjLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{
    // - emit target
    //    - assign this
    // - emit args
    // - do call

    // The call target is fully evaluated before the argument list. Note that we're not handling
    // put-call cases here currently, as such cases only apply to host objects
    // and are very unlikely to behave differently depending on the order of evaluation.

    switch (pnodeTarget->nop)
    {
    case knopDot:
    {
        funcInfo->AcquireLoc(pnodeTarget);
        // Assign the call target operand(s), putting them into expression temps if necessary to protect
        // them from side-effects.
        if (fSideEffectArgs)
        {
            // Though we're done with target evaluation after this point, still protect opnd1 from
            // arg side-effects as it's the "this" pointer.
            SaveOpndValue(pnodeTarget->sxBin.pnode1, funcInfo);
        }

        if ((pnodeTarget->sxBin.pnode2->nop == knopName) && ((pnodeTarget->sxBin.pnode2->sxPid.PropertyIdFromNameNode() == Js::PropertyIds::apply) || (pnodeTarget->sxBin.pnode2->sxPid.PropertyIdFromNameNode() == Js::PropertyIds::call)))
        {
            pnodeTarget->sxBin.pnode1->SetIsCallApplyTargetLoad();
        }

        Emit(pnodeTarget->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
        Js::PropertyId propertyId = pnodeTarget->sxBin.pnode2->sxPid.PropertyIdFromNameNode();
        Js::RegSlot callObjLocation = pnodeTarget->sxBin.pnode1->location;
        Js::RegSlot protoLocation = callObjLocation;
        EmitSuperMethodBegin(pnodeTarget, byteCodeGenerator, funcInfo);
        EmitMethodFld(pnodeTarget, protoLocation, propertyId, byteCodeGenerator, funcInfo);

        // Function calls on the 'super' object should maintain current 'this' pointer
        *thisLocation = (pnodeTarget->sxBin.pnode1->nop == knopSuper) ? funcInfo->thisPointerRegister : pnodeTarget->sxBin.pnode1->location;
        break;
    }

    case knopIndex:
    {
        funcInfo->AcquireLoc(pnodeTarget);
        // Assign the call target operand(s), putting them into expression temps if necessary to protect
        // them from side-effects.
        if (fSideEffectArgs || !(ParseNode::Grfnop(pnodeTarget->sxBin.pnode2->nop) & fnopLeaf))
        {
            // Though we're done with target evaluation after this point, still protect opnd1 from
            // arg or opnd2 side-effects as it's the "this" pointer.
            SaveOpndValue(pnodeTarget->sxBin.pnode1, funcInfo);
        }
        Emit(pnodeTarget->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
        Emit(pnodeTarget->sxBin.pnode2, byteCodeGenerator, funcInfo, false);

        Js::RegSlot indexLocation = pnodeTarget->sxBin.pnode2->location;
        Js::RegSlot callObjLocation = pnodeTarget->sxBin.pnode1->location;
        Js::RegSlot protoLocation = callObjLocation;
        EmitSuperMethodBegin(pnodeTarget, byteCodeGenerator, funcInfo);
        EmitMethodElem(pnodeTarget, protoLocation, indexLocation, byteCodeGenerator);

        funcInfo->ReleaseLoc(pnodeTarget->sxBin.pnode2); // don't release indexLocation until after we use it.

        // Function calls on the 'super' object should maintain current 'this' pointer
        *thisLocation = (pnodeTarget->sxBin.pnode1->nop == knopSuper) ? funcInfo->thisPointerRegister : pnodeTarget->sxBin.pnode1->location;
        break;
    }

    case knopClassDecl:
    {
        Emit(pnodeTarget, byteCodeGenerator, funcInfo, false);
        // We won't always have an assigned this register (e.g. class expression calls.) We need undefined in this case.
        *thisLocation = funcInfo->thisPointerRegister == Js::Constants::NoRegister ? funcInfo->undefinedConstantRegister : funcInfo->thisPointerRegister;
        break;
    }

    case knopSuper:
    {
        Emit(pnodeTarget, byteCodeGenerator, funcInfo, false, /*isConstructorCall*/ true);  // reuse isConstructorCall ("new super()" is illegal)

        // Super calls should always use the new.target register unless we don't have one.
        // That could happen if we have an eval('super()') outside of a class constructor.
        if (funcInfo->newTargetRegister != Js::Constants::NoRegister)
        {
            *thisLocation = funcInfo->newTargetRegister;
        }
        else
        {
            *thisLocation = funcInfo->thisPointerRegister;
        }
        break;
    }

    case knopName:
    {
        funcInfo->AcquireLoc(pnodeTarget);
        // Assign the call target operand(s), putting them into expression temps if necessary to protect
        // them from side-effects.
        if (fSideEffectArgs)
        {
            SaveOpndValue(pnodeTarget, funcInfo);
        }
        byteCodeGenerator->EmitLoadInstance(pnodeTarget->sxPid.sym, pnodeTarget->sxPid.pid, thisLocation, callObjLocation, funcInfo);
        if (*callObjLocation != Js::Constants::NoRegister)
        {
            // Load the call target as a property of the instance.
            Js::PropertyId propertyId = pnodeTarget->sxPid.PropertyIdFromNameNode();
            EmitMethodFld(pnodeTarget, *callObjLocation, propertyId, byteCodeGenerator, funcInfo);
            break;
        }

        // FALL THROUGH to evaluate call target.
    }

    default:
        // Assign the call target operand(s), putting them into expression temps if necessary to protect
        // them from side-effects.
        Emit(pnodeTarget, byteCodeGenerator, funcInfo, false);
        *thisLocation = funcInfo->undefinedConstantRegister;
        break;
    }

    // "This" pointer should have been assigned by the above.
    Assert(*thisLocation != Js::Constants::NoRegister);
}

void EmitCallI(
    ParseNode *pnode,
    BOOL fEvaluateComponents,
    BOOL fIsPut,
    BOOL fIsEval,
    BOOL fHasNewTarget,
    uint32 actualArgCount,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    Js::ProfileId callSiteId,
    Js::AuxArray<uint32> *spreadIndices = nullptr)
{
    // Emit a call where the target is in a register, because it's either a local name or an expression we've
    // already evaluated.

    ParseNode *pnodeTarget = pnode->sxBin.pnode1;
    Js::OpCode op;
    Js::CallFlags callFlags = Js::CallFlags::CallFlags_None;
    uint spreadExtraAlloc = 0;

    Js::ArgSlot actualArgSlotCount = (Js::ArgSlot) actualArgCount;

    // check for integer overflow
    if ((size_t)actualArgSlotCount != actualArgCount)
    {
        Js::Throw::OutOfMemory();
    }

    if (fIsPut)
    {
        if (pnode->sxCall.spreadArgCount > 0)
        {
            // TODO(tcare): We are disallowing spread with CallIPut for the moment. See DEVDIV2: 876387
            //              When CallIPut is migrated to the CallIExtended layout, this can be removed.
            byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_CantAsgCall));
        }
        // Grab a tmp register for the call result.
        Js::RegSlot tmpReg = funcInfo->AcquireTmpRegister();
        byteCodeGenerator->Writer()->CallI(Js::OpCode::CallIFlags, tmpReg, pnodeTarget->location, actualArgSlotCount, callSiteId, Js::CallFlags::CallFlags_NewTarget);
        funcInfo->ReleaseTmpRegister(tmpReg);
    }
    else
    {
        if (fEvaluateComponents)
        {
            // Release the call target operands we assigned above. If we didn't assign them here,
            // we'll need them later, so we can't re-use them for the result of the call.
            funcInfo->ReleaseLoc(pnodeTarget);
        }
        // Grab a register for the call result.
        if (pnode->isUsed)
        {
            funcInfo->AcquireLoc(pnode);
        }

        if (fIsEval)
        {
            op = Js::OpCode::CallIExtendedFlags;
            callFlags = Js::CallFlags::CallFlags_ExtraArg;
        }
        else
        {
            bool isSuperCall = pnodeTarget->nop == knopSuper;

            if (isSuperCall)
            {
                callFlags = Js::CallFlags_New;
            }
            if (fHasNewTarget)
            {
                callFlags = (Js::CallFlags) (callFlags | Js::CallFlags::CallFlags_ExtraArg | Js::CallFlags::CallFlags_NewTarget);
            }

            if (pnode->sxCall.spreadArgCount > 0)
            {
                op = (isSuperCall || fHasNewTarget) ? Js::OpCode::CallIExtendedFlags : Js::OpCode::CallIExtended;
            }
            else
            {
                op = (isSuperCall || fHasNewTarget) ? Js::OpCode::CallIFlags : Js::OpCode::CallI;
            }
        }

        if (op == Js::OpCode::CallI || op == Js::OpCode::CallIFlags)
        {
            byteCodeGenerator->Writer()->CallI(op, pnode->location, pnodeTarget->location, actualArgSlotCount, callSiteId, callFlags);
        }
        else
        {
            uint spreadIndicesSize = 0;
            Js::CallIExtendedOptions options = Js::CallIExtended_None;

            if (pnode->sxCall.spreadArgCount > 0)
            {
                Assert(spreadIndices != nullptr);
                spreadExtraAlloc = spreadIndices->count * sizeof(uint32);
                spreadIndicesSize = sizeof(*spreadIndices) + spreadExtraAlloc;
                options = Js::CallIExtended_SpreadArgs;
            }

            byteCodeGenerator->Writer()->CallIExtended(op, pnode->location, pnodeTarget->location, actualArgSlotCount, options, spreadIndices, spreadIndicesSize, callSiteId, callFlags);
        }

        if (pnode->sxCall.spreadArgCount > 0)
        {
            Assert(spreadExtraAlloc != 0);
            AdeletePlus(byteCodeGenerator->GetAllocator(), spreadExtraAlloc, spreadIndices);
        }
    }
}

void EmitCallInstrNoEvalComponents(
    ParseNode *pnode,
    BOOL fIsPut,
    BOOL fIsEval,
    Js::RegSlot thisLocation,
    Js::RegSlot callObjLocation,
    uint32 actualArgCount,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    Js::ProfileId callSiteId,
    Js::AuxArray<uint32> *spreadIndices = nullptr)
{
    // Emit the call instruction. The call target is a reference at this point, and we evaluate
    // it as part of doing the actual call.
    // Note that we don't handle the (fEvaluateComponents == TRUE) case in this function.
    // (This function is only called on the !fEvaluateComponents branch in EmitCall.)

    ParseNode *pnodeTarget = pnode->sxBin.pnode1;

    switch (pnodeTarget->nop)
    {
    case knopDot:
    {
        Assert(pnodeTarget->sxBin.pnode2->nop == knopName);
        Js::PropertyId propertyId = pnodeTarget->sxBin.pnode2->sxPid.PropertyIdFromNameNode();

        EmitMethodFld(pnodeTarget, callObjLocation, propertyId, byteCodeGenerator, funcInfo);
        EmitCallI(pnode, /*fEvaluateComponents*/ FALSE, fIsPut, fIsEval, /*fHasNewTarget*/ FALSE, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
    }
    break;

    case knopIndex:
    {
        EmitMethodElem(pnodeTarget, pnodeTarget->sxBin.pnode1->location, pnodeTarget->sxBin.pnode2->location, byteCodeGenerator);
        EmitCallI(pnode, /*fEvaluateComponents*/ FALSE, fIsPut, fIsEval, /*fHasNewTarget*/ FALSE, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
    }
    break;

    case knopName:
    {
        if (callObjLocation != Js::Constants::NoRegister)
        {
            // We still have to get the property from its instance, so emit CallFld.
            if (thisLocation != callObjLocation)
            {
                funcInfo->ReleaseTmpRegister(thisLocation);
            }
            funcInfo->ReleaseTmpRegister(callObjLocation);

            Js::PropertyId propertyId = pnodeTarget->sxPid.PropertyIdFromNameNode();
            EmitMethodFld(pnodeTarget, callObjLocation, propertyId, byteCodeGenerator, funcInfo);
            EmitCallI(pnode, /*fEvaluateComponents*/ FALSE, fIsPut, fIsEval, /*fHasNewTarget*/ FALSE, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
            break;
        }
    }
    // FALL THROUGH

    default:
        EmitCallI(pnode, /*fEvaluateComponents*/ FALSE, fIsPut, fIsEval, /*fHasNewTarget*/ FALSE, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
        break;
    }
}

void EmitCallInstr(
    ParseNode *pnode,
    BOOL fIsPut,
    BOOL fIsEval,
    BOOL fHasNewTarget,
    Js::RegSlot thisLocation,
    Js::RegSlot callObjLocation,
    uint32 actualArgCount,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    Js::ProfileId callSiteId,
    Js::AuxArray<uint32> *spreadIndices = nullptr)
{
    // Emit a call instruction. The call target has been fully evaluated already, so we always
    // emit a CallI through the register that holds the target value.
    // Note that we don't handle !fEvaluateComponents cases at this point.
    // (This function is only called on the fEvaluateComponents branch in EmitCall.)

    if (thisLocation != Js::Constants::NoRegister)
    {
        funcInfo->ReleaseTmpRegister(thisLocation);
    }

    if (callObjLocation != Js::Constants::NoRegister &&
        callObjLocation != thisLocation)
    {
        funcInfo->ReleaseTmpRegister(callObjLocation);
    }

    EmitCallI(pnode, /*fEvaluateComponents*/ TRUE, fIsPut, fIsEval, fHasNewTarget, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
}

void EmitCall(
    ParseNode* pnode,
    Js::RegSlot rhsLocation,
    ByteCodeGenerator* byteCodeGenerator,
    FuncInfo* funcInfo,
    BOOL fReturnValue,
    BOOL fEvaluateComponents,
    BOOL fHasNewTarget,
    Js::RegSlot overrideThisLocation)
{
    BOOL fIsPut = (rhsLocation != Js::Constants::NoRegister);
    // If the call returns a float, we'll note this in the byte code.
    Js::RegSlot thisLocation = Js::Constants::NoRegister;
    Js::RegSlot callObjLocation = Js::Constants::NoRegister;
    Js::RegSlot newTargetLocation = Js::Constants::NoRegister;
    BOOL fSideEffectArgs = FALSE;
    ParseNode *pnodeTarget = pnode->sxCall.pnodeTarget;
    ParseNode *pnodeArgs = pnode->sxCall.pnodeArgs;
    uint16 spreadArgCount = pnode->sxCall.spreadArgCount;

    unsigned int argCount = CountArguments(pnode->sxCall.pnodeArgs, &fSideEffectArgs) + (unsigned int)fIsPut;

    BOOL fIsEval = !fIsPut && pnode->sxCall.isEvalCall;

    if (fIsEval)
    {
        Assert(!fHasNewTarget);

        //
        // "eval" takes the closure environment as an extra argument
        // Pass the closure env only if some argument is passed
        // For just eval(), don't pass the closure environment
        //
        if (argCount > 1)
        {
            // Check the module ID as well. If it's not the global (default) module,
            // we need to pass the root to eval so it can do the right global lookups.
            // (Passing the module root is the least disruptive way to get the module ID
            // to the helper, given the current set of byte codes. Once we have a full set
            // of byte code ops taking immediate opnds, passing the ID is more intuitive.)
            Js::ModuleID moduleID = byteCodeGenerator->GetModuleID();
            if (moduleID == kmodGlobal)
            {
                argCount++;
            }
            else
            {
                // Module ID must be passed
                argCount += 2;
            }
        }
    }

    if (fHasNewTarget)
    {
        Assert(!fIsEval);

        // When we need to pass new.target explicitly, it is passed as an extra argument.
        // This is similar to how eval passes an extra argument for the frame display and is
        // used to support cases where we need to pass both 'this' and new.target as part of
        // a function call.
        // OpCode::LdNewTarget knows how to look at the call flags and fetch this argument.
        argCount++;
        newTargetLocation = funcInfo->newTargetRegister;

        Assert(newTargetLocation != Js::Constants::NoRegister);
    }

    Js::ArgSlot argSlotCount = (Js::ArgSlot)argCount;

    if (argCount != (unsigned int)argSlotCount)
    {
        Js::Throw::OutOfMemory();
    }

    if (fReturnValue)
    {
        pnode->isUsed = true;
    }

    //
    // Set up the call.
    //

    if (!fEvaluateComponents)
    {
        EmitCallTargetNoEvalComponents(pnodeTarget, fSideEffectArgs, &thisLocation, &callObjLocation, byteCodeGenerator, funcInfo);
    }
    else
    {
        EmitCallTarget(pnodeTarget, fSideEffectArgs, &thisLocation, &callObjLocation, byteCodeGenerator, funcInfo);
    }

    bool releaseThisLocation = true;
    // If we are strictly overriding the this location, ignore what the call target set this location to.
    if (overrideThisLocation != Js::Constants::NoRegister)
    {
        thisLocation = overrideThisLocation;
        releaseThisLocation = false;
    }

    // Evaluate the arguments (nothing mode-specific here).
    // Start call, allocate out param space
    funcInfo->StartRecordingOutArgs(argSlotCount);

    Js::ProfileId callSiteId = byteCodeGenerator->GetNextCallSiteId(Js::OpCode::CallI);

    byteCodeGenerator->Writer()->StartCall(Js::OpCode::StartCall, argSlotCount);
    Js::AuxArray<uint32> *spreadIndices;
    Js::ArgSlot actualArgCount = EmitArgList(pnodeArgs, rhsLocation, thisLocation, newTargetLocation, fIsEval, fEvaluateComponents, byteCodeGenerator, funcInfo, callSiteId, spreadArgCount, &spreadIndices);
    Assert(argSlotCount == actualArgCount);

    if (!fEvaluateComponents)
    {
        EmitCallInstrNoEvalComponents(pnode, fIsPut, fIsEval, thisLocation, callObjLocation, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
    }
    else
    {
        EmitCallInstr(pnode, fIsPut, fIsEval, fHasNewTarget, releaseThisLocation ? thisLocation : Js::Constants::NoRegister, callObjLocation, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
    }

    // End call, pop param space
    funcInfo->EndRecordingOutArgs(argSlotCount);
}

void EmitInvoke(
    Js::RegSlot location,
    Js::RegSlot callObjLocation,
    Js::PropertyId propertyId,
    ByteCodeGenerator* byteCodeGenerator,
    FuncInfo* funcInfo)
{
    EmitMethodFld(false, false, location, callObjLocation, propertyId, byteCodeGenerator, funcInfo);

    funcInfo->StartRecordingOutArgs(1);

    Js::ProfileId callSiteId = byteCodeGenerator->GetNextCallSiteId(Js::OpCode::CallI);

    byteCodeGenerator->Writer()->StartCall(Js::OpCode::StartCall, 1);
    EmitArgListStart(callObjLocation, byteCodeGenerator, funcInfo, callSiteId);

    byteCodeGenerator->Writer()->CallI(Js::OpCode::CallI, location, location, 1, callSiteId);
}

void EmitInvoke(
    Js::RegSlot location,
    Js::RegSlot callObjLocation,
    Js::PropertyId propertyId,
    ByteCodeGenerator* byteCodeGenerator,
    FuncInfo* funcInfo,
    Js::RegSlot arg1Location)
{
    EmitMethodFld(false, false, location, callObjLocation, propertyId, byteCodeGenerator, funcInfo);

    funcInfo->StartRecordingOutArgs(2);

    Js::ProfileId callSiteId = byteCodeGenerator->GetNextCallSiteId(Js::OpCode::CallI);

    byteCodeGenerator->Writer()->StartCall(Js::OpCode::StartCall, 2);
    EmitArgListStart(callObjLocation, byteCodeGenerator, funcInfo, callSiteId);
    byteCodeGenerator->Writer()->ArgOut<true>(1, arg1Location, callSiteId);

    byteCodeGenerator->Writer()->CallI(Js::OpCode::CallI, location, location, 2, callSiteId);
}

void EmitComputedFunctionNameVar(ParseNode *nameNode, ParseNode *exprNode, ByteCodeGenerator *byteCodeGenerator)
{
    AssertMsg(exprNode != nullptr, "callers of this function should pass in a valid expression Node");

    if (nameNode == nullptr)
    {
        return;
    }

    if ((exprNode->nop == knopFncDecl && (exprNode->sxFnc.pnodeName == nullptr || exprNode->sxFnc.pnodeName->nop != knopVarDecl)))
    {
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::SetComputedNameVar, exprNode->location, nameNode->location);
    }
}

void EmitMemberNode(ParseNode *memberNode, Js::RegSlot objectLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, ParseNode* parentNode, bool useStore, bool* isObjectEmpty = nullptr)
{
    ParseNode *nameNode = memberNode->sxBin.pnode1;
    ParseNode *exprNode = memberNode->sxBin.pnode2;

    bool isFncDecl = exprNode->nop == knopFncDecl;
    bool isClassMember = isFncDecl && exprNode->sxFnc.IsClassMember();

    // Moved SetComputedNameVar before LdFld of prototype because loading the prototype undefers the function TypeHandler
    // which makes this bytecode too late to influence the function.name.
    if (nameNode->nop == knopComputedName)
    {
        // Computed property name
        // Transparently pass the name expr
        // The Emit will replace this with a temp register if necessary to preserve the value.
        nameNode->location = nameNode->sxUni.pnode1->location;
        EmitBinaryOpnds(nameNode, exprNode, byteCodeGenerator, funcInfo);
        if (isFncDecl && !exprNode->sxFnc.IsClassConstructor())
        {
            EmitComputedFunctionNameVar(nameNode, exprNode, byteCodeGenerator);
        }
    }

    // Classes allocates a RegSlot as part of Instance Methods EmitClassInitializers,
    // but if we don't have any members then we don't need to load the prototype.
    Assert(isClassMember == (isObjectEmpty != nullptr));
    if (isClassMember && *isObjectEmpty)
    {
        *isObjectEmpty = false;
        int cacheId = funcInfo->FindOrAddInlineCacheId(parentNode->location, Js::PropertyIds::prototype, false, false);
        byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, objectLocation, parentNode->location, cacheId);
    }

    if (nameNode->nop == knopComputedName)
    {
        Assert(memberNode->nop == knopGetMember || memberNode->nop == knopSetMember || memberNode->nop == knopMember);

        Js::OpCode setOp = memberNode->nop == knopGetMember ?
            (isClassMember ? Js::OpCode::InitClassMemberGetComputedName : Js::OpCode::InitGetElemI) :
            memberNode->nop == knopSetMember ?
            (isClassMember ? Js::OpCode::InitClassMemberSetComputedName : Js::OpCode::InitSetElemI) :
            (isClassMember ? Js::OpCode::InitClassMemberComputedName : Js::OpCode::InitComputedProperty);

        byteCodeGenerator->Writer()->Element(setOp, exprNode->location, objectLocation, nameNode->location, true);

        // Class and object members need a reference back to the class.
        if (isFncDecl)
        {
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::SetHomeObj, exprNode->location, objectLocation);
        }

        funcInfo->ReleaseLoc(exprNode);
        funcInfo->ReleaseLoc(nameNode);

        return;
    }

    Js::OpCode stFldOpCode = (Js::OpCode)0;
    if (useStore)
    {
        stFldOpCode = ByteCodeGenerator::GetStFldOpCode(funcInfo, false, false, false, isClassMember);
    }

    Emit(exprNode, byteCodeGenerator, funcInfo, false);
    Js::PropertyId propertyId = nameNode->sxPid.PropertyIdFromNameNode();

    if (Js::PropertyIds::name == propertyId
        && exprNode->nop == knopFncDecl
        && exprNode->sxFnc.IsStaticMember()
        && parentNode != nullptr && parentNode->nop == knopClassDecl
        && parentNode->sxClass.pnodeConstructor != nullptr)
    {
        Js::ParseableFunctionInfo* nameFunc = parentNode->sxClass.pnodeConstructor->sxFnc.funcInfo->byteCodeFunction->GetParseableFunctionInfo();
        nameFunc->SetIsStaticNameFunction(true);
    }

    if (memberNode->nop == knopMember || memberNode->nop == knopMemberShort)
    {
        // The internal prototype should be set only if the production is of the form PropertyDefinition : PropertyName : AssignmentExpression
        if (propertyId == Js::PropertyIds::__proto__ && memberNode->nop != knopMemberShort && (exprNode->nop != knopFncDecl || !exprNode->sxFnc.IsMethod()))
        {
            byteCodeGenerator->Writer()->Property(Js::OpCode::InitProto, exprNode->location, objectLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));
        }
        else
        {
            uint cacheId = funcInfo->FindOrAddInlineCacheId(objectLocation, propertyId, false, true);
            Js::OpCode patchablePropertyOpCode;

            if (useStore)
            {
                patchablePropertyOpCode = stFldOpCode;
            }
            else if (isClassMember)
            {
                patchablePropertyOpCode = Js::OpCode::InitClassMember;
            }
            else
            {
                patchablePropertyOpCode = Js::OpCode::InitFld;
            }

            byteCodeGenerator->Writer()->PatchableProperty(patchablePropertyOpCode, exprNode->location, objectLocation, cacheId);
        }
    }
    else
    {
        Assert(memberNode->nop == knopGetMember || memberNode->nop == knopSetMember);

        Js::OpCode setOp = memberNode->nop == knopGetMember ?
            (isClassMember ? Js::OpCode::InitClassMemberGet : Js::OpCode::InitGetFld) :
            (isClassMember ? Js::OpCode::InitClassMemberSet : Js::OpCode::InitSetFld);

        byteCodeGenerator->Writer()->Property(setOp, exprNode->location, objectLocation, funcInfo->FindOrAddReferencedPropertyId(propertyId));
    }

    // Class and object members need a reference back to the class.
    if (isFncDecl)
    {
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::SetHomeObj, exprNode->location, objectLocation);
    }

    funcInfo->ReleaseLoc(exprNode);

    if (propertyId == Js::PropertyIds::valueOf)
    {
        byteCodeGenerator->GetScriptContext()->optimizationOverrides.SetSideEffects(Js::SideEffects_ValueOf);
    }
    else if (propertyId == Js::PropertyIds::toString)
    {
        byteCodeGenerator->GetScriptContext()->optimizationOverrides.SetSideEffects(Js::SideEffects_ToString);
    }
}

void EmitClassInitializers(ParseNode *memberList, Js::RegSlot objectLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, ParseNode* parentNode, bool isObjectEmpty)
{
    if (memberList != nullptr)
    {
        while (memberList->nop == knopList)
        {
            ParseNode *memberNode = memberList->sxBin.pnode1;
            EmitMemberNode(memberNode, objectLocation, byteCodeGenerator, funcInfo, parentNode, /*useStore*/ false, &isObjectEmpty);
            memberList = memberList->sxBin.pnode2;
        }
        EmitMemberNode(memberList, objectLocation, byteCodeGenerator, funcInfo, parentNode, /*useStore*/ false, &isObjectEmpty);
    }
}

void EmitObjectInitializers(ParseNode *memberList, Js::RegSlot objectLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    ParseNode *pmemberList = memberList;
    unsigned int argCount = 0;
    uint32  value;
    Js::PropertyId propertyId;

    //
    // 1. Add all non-int property ids to a dictionary propertyIds with value true
    // 2. Get the count of propertyIds
    // 3. Create a propertyId array of size count
    // 4. Put the propIds in the auxiliary area
    // 5. Get the objectLiteralCacheId
    // 6. Generate propId inits with values
    //

    // Handle propertyId collision
    typedef JsUtil::BaseHashSet<Js::PropertyId, ArenaAllocator, PowerOf2SizePolicy> PropertyIdSet;
    PropertyIdSet* propertyIds = Anew(byteCodeGenerator->GetAllocator(), PropertyIdSet, byteCodeGenerator->GetAllocator(), 17);

    bool hasComputedName = false;
    if (memberList != nullptr)
    {
        while (memberList->nop == knopList)
        {
            if (memberList->sxBin.pnode1->sxBin.pnode1->nop == knopComputedName)
            {
                hasComputedName = true;
                break;
            }

            propertyId = memberList->sxBin.pnode1->sxBin.pnode1->sxPid.PropertyIdFromNameNode();
            if (!byteCodeGenerator->GetScriptContext()->IsNumericPropertyId(propertyId, &value))
            {
                propertyIds->Item(propertyId);
            }

            memberList = memberList->sxBin.pnode2;
        }

        if (memberList->sxBin.pnode1->nop != knopComputedName && !hasComputedName)
        {
            propertyId = memberList->sxBin.pnode1->sxPid.PropertyIdFromNameNode();
            if (!byteCodeGenerator->GetScriptContext()->IsNumericPropertyId(propertyId, &value))
            {
                propertyIds->Item(propertyId);
            }
        }
    }

    argCount = propertyIds->Count();

    memberList = pmemberList;
    if ((memberList == nullptr) || (argCount == 0))
    {
        // Empty literal or numeric property only object literal
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::NewScObjectSimple, objectLocation);
    }
    else
    {
        Js::PropertyIdArray *propIds = AnewPlus(byteCodeGenerator->GetAllocator(), argCount * sizeof(Js::PropertyId), Js::PropertyIdArray, argCount);

        if (propertyIds->ContainsKey(Js::PropertyIds::__proto__))
        {
            // Always record whether the initializer contains __proto__ no matter if current environment has it enabled
            // or not, in case the bytecode is later run with __proto__ enabled.
            propIds->has__proto__ = true;
        }

        unsigned int argIndex = 0;
        while (memberList->nop == knopList)
        {
            if (memberList->sxBin.pnode1->sxBin.pnode1->nop == knopComputedName)
            {
                break;
            }
            propertyId = memberList->sxBin.pnode1->sxBin.pnode1->sxPid.PropertyIdFromNameNode();
            if (!byteCodeGenerator->GetScriptContext()->IsNumericPropertyId(propertyId, &value) && propertyIds->Remove(propertyId))
            {
                propIds->elements[argIndex] = propertyId;
                argIndex++;
            }
            memberList = memberList->sxBin.pnode2;
        }

        if (memberList->sxBin.pnode1->nop != knopComputedName && !hasComputedName)
        {
            propertyId = memberList->sxBin.pnode1->sxPid.PropertyIdFromNameNode();
            if (!byteCodeGenerator->GetScriptContext()->IsNumericPropertyId(propertyId, &value) && propertyIds->Remove(propertyId))
            {
                propIds->elements[argIndex] = propertyId;
                argIndex++;
            }
        }

        uint32 literalObjectId = funcInfo->GetParsedFunctionBody()->NewObjectLiteral();

        // Generate the opcode with propIds and cacheId
        byteCodeGenerator->Writer()->Auxiliary(Js::OpCode::NewScObjectLiteral, objectLocation, propIds, sizeof(Js::PropertyIdArray) + argCount * sizeof(Js::PropertyId), literalObjectId);

        Adelete(byteCodeGenerator->GetAllocator(), propertyIds);

        AdeletePlus(byteCodeGenerator->GetAllocator(), argCount * sizeof(Js::PropertyId), propIds);
    }

    memberList = pmemberList;

    bool useStore = false;
    // Generate the actual assignment to those properties
    if (memberList != nullptr)
    {
        while (memberList->nop == knopList)
        {
            ParseNode *memberNode = memberList->sxBin.pnode1;

            if (memberNode->sxBin.pnode1->nop == knopComputedName)
            {
                useStore = true;
            }

            byteCodeGenerator->StartSubexpression(memberNode);
            EmitMemberNode(memberNode, objectLocation, byteCodeGenerator, funcInfo, nullptr, useStore);
            byteCodeGenerator->EndSubexpression(memberNode);
            memberList = memberList->sxBin.pnode2;
        }

        byteCodeGenerator->StartSubexpression(memberList);
        EmitMemberNode(memberList, objectLocation, byteCodeGenerator, funcInfo, nullptr, useStore);
        byteCodeGenerator->EndSubexpression(memberList);
    }
}

void EmitStringTemplate(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    Assert(pnode->sxStrTemplate.pnodeStringLiterals);

    // For a tagged string template, we will create the callsite constant object as part of the FunctionBody constants table.
    // We only need to emit code for non-tagged string templates here.
    if (!pnode->sxStrTemplate.isTaggedTemplate)
    {
        // If we have no substitutions and this is not a tagged template, we can emit just the single cooked string.
        if (pnode->sxStrTemplate.pnodeSubstitutionExpressions == nullptr)
        {
            Assert(pnode->sxStrTemplate.pnodeStringLiterals->nop != knopList);

            funcInfo->AcquireLoc(pnode);
            Emit(pnode->sxStrTemplate.pnodeStringLiterals, byteCodeGenerator, funcInfo, false);

            Assert(pnode->location != pnode->sxStrTemplate.pnodeStringLiterals->location);

            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxStrTemplate.pnodeStringLiterals->location);
            funcInfo->ReleaseLoc(pnode->sxStrTemplate.pnodeStringLiterals);
        }
        else
        {
            // If we have substitutions but no tag function, we can skip the callSite object construction (and also ignore raw strings).
            funcInfo->AcquireLoc(pnode);

            // First string must be a list node since we have substitutions.
            AssertMsg(pnode->sxStrTemplate.pnodeStringLiterals->nop == knopList, "First string in the list must be a knopList node.");

            ParseNode* stringNodeList = pnode->sxStrTemplate.pnodeStringLiterals;

            // Emit the first string and load that into the pnode location.
            Emit(stringNodeList->sxBin.pnode1, byteCodeGenerator, funcInfo, false);

            Assert(pnode->location != stringNodeList->sxBin.pnode1->location);

            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, stringNodeList->sxBin.pnode1->location);
            funcInfo->ReleaseLoc(stringNodeList->sxBin.pnode1);

            ParseNode* expressionNodeList = pnode->sxStrTemplate.pnodeSubstitutionExpressions;
            ParseNode* stringNode;
            ParseNode* expressionNode;

            // Now append the substitution expressions and remaining string constants via normal add operator
            // We will always have one more string constant than substitution expression
            // `strcon1 ${expr1} strcon2 ${expr2} strcon3` = strcon1 + expr1 + strcon2 + expr2 + strcon3
            //
            // strcon1 --- step 1 (above)
            // expr1   \__ step 2
            // strcon2 /
            // expr2   \__ step 3
            // strcon3 /
            while (stringNodeList->nop == knopList)
            {
                // If the current head of the expression list is a list, fetch the node and walk the list.
                if (expressionNodeList->nop == knopList)
                {
                    expressionNode = expressionNodeList->sxBin.pnode1;
                    expressionNodeList = expressionNodeList->sxBin.pnode2;
                }
                else
                {
                    // This is the last element of the expression list.
                    expressionNode = expressionNodeList;
                }

                // Emit the expression and append it to the string we're building.
                Emit(expressionNode, byteCodeGenerator, funcInfo, false);

                Js::RegSlot toStringLocation = funcInfo->AcquireTmpRegister();
                byteCodeGenerator->Writer()->Reg2(Js::OpCode::Conv_Str, toStringLocation, expressionNode->location);
                byteCodeGenerator->Writer()->Reg3(Js::OpCode::Add_A, pnode->location, pnode->location, toStringLocation);
                funcInfo->ReleaseTmpRegister(toStringLocation);
                funcInfo->ReleaseLoc(expressionNode);

                // Move to the next string in the list - we already got ahead of the expressions in the first string literal above.
                stringNodeList = stringNodeList->sxBin.pnode2;

                // If the current head of the string literal list is also a list node, need to fetch the actual string literal node.
                if (stringNodeList->nop == knopList)
                {
                    stringNode = stringNodeList->sxBin.pnode1;
                }
                else
                {
                    // This is the last element of the string literal list.
                    stringNode = stringNodeList;
                }

                // Emit the string node following the previous expression and append it to the string.
                // This is either just some string in the list or it is the last string.
                Emit(stringNode, byteCodeGenerator, funcInfo, false);
                byteCodeGenerator->Writer()->Reg3(Js::OpCode::Add_A, pnode->location, pnode->location, stringNode->location);
                funcInfo->ReleaseLoc(stringNode);
            }
        }
    }
}

void SetNewArrayElements(ParseNode *pnode, Js::RegSlot arrayLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    ParseNode *args = pnode->sxUni.pnode1;
    uint argCount = pnode->sxArrLit.count;
    uint spreadCount = pnode->sxArrLit.spreadCount;
    bool nativeArrays = CreateNativeArrays(byteCodeGenerator, funcInfo);

    bool arrayIntOpt = nativeArrays && pnode->sxArrLit.arrayOfInts;
    if (arrayIntOpt)
    {
        int extraAlloc = argCount * sizeof(int32);
        Js::AuxArray<int> *ints = AnewPlus(byteCodeGenerator->GetAllocator(), extraAlloc, Js::AuxArray<int32>, argCount);
        EmitConstantArgsToIntArray(byteCodeGenerator, ints->elements, args, argCount);
        Assert(!pnode->sxArrLit.hasMissingValues);
        byteCodeGenerator->Writer()->Auxiliary(
            Js::OpCode::NewScIntArray,
            pnode->location,
            ints,
            sizeof(Js::AuxArray<int>) + extraAlloc,
            argCount);
        AdeletePlus(byteCodeGenerator->GetAllocator(), extraAlloc, ints);
        return;
    }

    bool arrayNumOpt = nativeArrays && pnode->sxArrLit.arrayOfNumbers;
    if (arrayNumOpt)
    {
        int extraAlloc = argCount * sizeof(double);
        Js::AuxArray<double> *doubles = AnewPlus(byteCodeGenerator->GetAllocator(), extraAlloc, Js::AuxArray<double>, argCount);
        EmitConstantArgsToFltArray(byteCodeGenerator, doubles->elements, args, argCount);
        Assert(!pnode->sxArrLit.hasMissingValues);
        byteCodeGenerator->Writer()->Auxiliary(
            Js::OpCode::NewScFltArray,
            pnode->location,
            doubles,
            sizeof(Js::AuxArray<double>) + extraAlloc,
            argCount);
        AdeletePlus(byteCodeGenerator->GetAllocator(), extraAlloc, doubles);
        return;
    }

    bool arrayLitOpt = pnode->sxArrLit.arrayOfTaggedInts && pnode->sxArrLit.count > 1;
    Assert(!arrayLitOpt || !nativeArrays);

    Js::RegSlot spreadArrLoc = arrayLocation;
    Js::AuxArray<uint32> *spreadIndices = nullptr;
    const uint extraAlloc = spreadCount * sizeof(uint32);
    if (pnode->sxArrLit.spreadCount > 0)
    {
        arrayLocation = funcInfo->AcquireTmpRegister();
        spreadIndices = AnewPlus(byteCodeGenerator->GetAllocator(), extraAlloc, Js::AuxArray<uint32>, spreadCount);
    }

    byteCodeGenerator->Writer()->Reg1Unsigned1(
        pnode->sxArrLit.hasMissingValues ? Js::OpCode::NewScArrayWithMissingValues : Js::OpCode::NewScArray,
        arrayLocation,
        argCount);

    if (args != nullptr)
    {
        Js::OpCode opcode;
        Js::RegSlot arrLoc;
        if (argCount == 1 && !byteCodeGenerator->Writer()->DoProfileNewScArrayOp(Js::OpCode::NewScArray))
        {
            opcode = Js::OpCode::StArrItemC_CI4;
            arrLoc = arrayLocation;
        }
        else if (arrayLitOpt)
        {
            opcode = Js::OpCode::StArrSegItem_A;
            arrLoc = funcInfo->AcquireTmpRegister();
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::LdArrHead, arrLoc, arrayLocation);
        }
        else if (Js::JavascriptArray::HasInlineHeadSegment(argCount))
        {
            // The head segment will be allocated inline as an interior pointer. To keep the array alive, the set operation
            // should be done relative to the array header to keep it alive (instead of the array segment).
            opcode = Js::OpCode::StArrInlineItem_CI4;
            arrLoc = arrayLocation;
        }
        else if (argCount <= Js::JavascriptArray::MaxInitialDenseLength)
        {
            opcode = Js::OpCode::StArrSegItem_CI4;
            arrLoc = funcInfo->AcquireTmpRegister();
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::LdArrHead, arrLoc, arrayLocation);
        }
        else
        {
            opcode = Js::OpCode::StArrItemI_CI4;
            arrLoc = arrayLocation;
        }

        if (arrayLitOpt)
        {
            Js::VarArray *vars = AnewPlus(byteCodeGenerator->GetAllocator(), argCount * sizeof(Js::Var), Js::VarArray, argCount);

            EmitConstantArgsToVarArray(byteCodeGenerator, vars->elements, args, argCount);

            // Generate the opcode with vars
            byteCodeGenerator->Writer()->Auxiliary(Js::OpCode::StArrSegItem_A, arrLoc, vars, sizeof(Js::VarArray) + argCount * sizeof(Js::Var), argCount);

            AdeletePlus(byteCodeGenerator->GetAllocator(), argCount * sizeof(Js::Var), vars);
        }
        else
        {
            uint i = 0;
            unsigned spreadIndex = 0;
            Js::RegSlot rhsLocation;
            while (args->nop == knopList)
            {
                if (args->sxBin.pnode1->nop != knopEmpty)
                {
                    Emit(args->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
                    rhsLocation = args->sxBin.pnode1->location;
                    Js::RegSlot regVal = rhsLocation;
                    if (args->sxBin.pnode1->nop == knopEllipsis)
                    {
                        AnalysisAssert(spreadIndices);
                        regVal = funcInfo->AcquireTmpRegister();
                        byteCodeGenerator->Writer()->Reg2(Js::OpCode::LdCustomSpreadIteratorList, regVal, rhsLocation);
                        spreadIndices->elements[spreadIndex++] = i;
                    }

                    byteCodeGenerator->Writer()->ElementUnsigned1(opcode, regVal, arrLoc, i);

                    if (args->sxBin.pnode1->nop == knopEllipsis)
                    {
                        funcInfo->ReleaseTmpRegister(regVal);
                    }

                    funcInfo->ReleaseLoc(args->sxBin.pnode1);
                }

                args = args->sxBin.pnode2;
                i++;
            }

            if (args->nop != knopEmpty)
            {
                Emit(args, byteCodeGenerator, funcInfo, false);
                rhsLocation = args->location;
                Js::RegSlot regVal = rhsLocation;
                if (args->nop == knopEllipsis)
                {
                    regVal = funcInfo->AcquireTmpRegister();
                    byteCodeGenerator->Writer()->Reg2(Js::OpCode::LdCustomSpreadIteratorList, regVal, rhsLocation);
                    AnalysisAssert(spreadIndices);
                    spreadIndices->elements[spreadIndex] = i;
                }

                byteCodeGenerator->Writer()->ElementUnsigned1(opcode, regVal, arrLoc, i);

                if (args->nop == knopEllipsis)
                {
                    funcInfo->ReleaseTmpRegister(regVal);
                }

                funcInfo->ReleaseLoc(args);
                i++;
            }
            Assert(i <= argCount);
        }

        if (arrLoc != arrayLocation)
        {
            funcInfo->ReleaseTmpRegister(arrLoc);
        }
    }

    if (pnode->sxArrLit.spreadCount > 0)
    {
        byteCodeGenerator->Writer()->Reg2Aux(Js::OpCode::SpreadArrayLiteral, spreadArrLoc, arrayLocation, spreadIndices, sizeof(Js::AuxArray<uint32>) + extraAlloc, extraAlloc);
        AdeletePlus(byteCodeGenerator->GetAllocator(), extraAlloc, spreadIndices);
        funcInfo->ReleaseTmpRegister(arrayLocation);
    }
}

// FIX: TODO: mixed-mode expressions (arithmetic expressions mixed with boolean expressions); current solution
// will not short-circuit in some cases and is not complete (for example: var i=(x==y))
// This uses Aho and Ullman style double-branch generation (p. 494 ASU); we will need to peephole optimize or replace
// with special case for single-branch style.
void EmitBooleanExpression(ParseNode *expr, Js::ByteCodeLabel trueLabel, Js::ByteCodeLabel falseLabel, ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{
    switch (expr->nop)
    {

    case knopLogOr:
    {
        byteCodeGenerator->StartStatement(expr);
        Js::ByteCodeLabel leftFalse = byteCodeGenerator->Writer()->DefineLabel();
        EmitBooleanExpression(expr->sxBin.pnode1, trueLabel, leftFalse, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode1);
        byteCodeGenerator->Writer()->MarkLabel(leftFalse);
        EmitBooleanExpression(expr->sxBin.pnode2, trueLabel, falseLabel, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode2);
        byteCodeGenerator->EndStatement(expr);
        break;
    }

    case knopLogAnd:
    {
        byteCodeGenerator->StartStatement(expr);
        Js::ByteCodeLabel leftTrue = byteCodeGenerator->Writer()->DefineLabel();
        EmitBooleanExpression(expr->sxBin.pnode1, leftTrue, falseLabel, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode1);
        byteCodeGenerator->Writer()->MarkLabel(leftTrue);
        EmitBooleanExpression(expr->sxBin.pnode2, trueLabel, falseLabel, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode2);
        byteCodeGenerator->EndStatement(expr);
        break;
    }

    case knopLogNot:
        byteCodeGenerator->StartStatement(expr);
        EmitBooleanExpression(expr->sxUni.pnode1, falseLabel, trueLabel, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxUni.pnode1);
        byteCodeGenerator->EndStatement(expr);
        break;

    case knopEq:
    case knopEqv:
    case knopNEqv:
    case knopNe:
    case knopLt:
    case knopLe:
    case knopGe:
    case knopGt:
        byteCodeGenerator->StartStatement(expr);
        EmitBinaryOpnds(expr->sxBin.pnode1, expr->sxBin.pnode2, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode2);
        funcInfo->ReleaseLoc(expr->sxBin.pnode1);
        byteCodeGenerator->Writer()->BrReg2(nopToOp[expr->nop], trueLabel, expr->sxBin.pnode1->location,
            expr->sxBin.pnode2->location);
        byteCodeGenerator->Writer()->Br(falseLabel);
        byteCodeGenerator->EndStatement(expr);
        break;
    case knopTrue:
        byteCodeGenerator->StartStatement(expr);
        byteCodeGenerator->Writer()->Br(trueLabel);
        byteCodeGenerator->EndStatement(expr);
        break;
    case knopFalse:
        byteCodeGenerator->StartStatement(expr);
        byteCodeGenerator->Writer()->Br(falseLabel);
        byteCodeGenerator->EndStatement(expr);
        break;
    default:
        // Note: we usually release the temp assigned to a node after we Emit it.
        // But in this case, EmitBooleanExpression is just a wrapper around a normal Emit call,
        // and the caller of EmitBooleanExpression expects to be able to release this register.

        // For diagnostics purposes, register the name and dot to the statement list.
        if (expr->nop == knopName || expr->nop == knopDot)
        {
            byteCodeGenerator->StartStatement(expr);
            Emit(expr, byteCodeGenerator, funcInfo, false);
            byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, trueLabel, expr->location);
            byteCodeGenerator->Writer()->Br(falseLabel);
            byteCodeGenerator->EndStatement(expr);
        }
        else
        {
            Emit(expr, byteCodeGenerator, funcInfo, false);
            byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, trueLabel, expr->location);
            byteCodeGenerator->Writer()->Br(falseLabel);
        }
        break;
    }
}

// used by while and for loops
void EmitLoop(
    ParseNode *loopNode,
    ParseNode *cond,
    ParseNode *body,
    ParseNode *incr,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    BOOL fReturnValue,
    BOOL doWhile = FALSE,
    ParseNode *forLoopBlock = nullptr)
{
    // Need to increment loop count whether we are going to profile or not for HasLoop()

    Js::ByteCodeLabel loopEntrance = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel continuePastLoop = byteCodeGenerator->Writer()->DefineLabel();

    uint loopId = byteCodeGenerator->Writer()->EnterLoop(loopEntrance);
    loopNode->sxLoop.loopId = loopId;

    if (doWhile)
    {
        Emit(body, byteCodeGenerator, funcInfo, fReturnValue);
        funcInfo->ReleaseLoc(body);
        if (loopNode->emitLabels)
        {
            byteCodeGenerator->Writer()->MarkLabel(loopNode->sxStmt.continueLabel);
        }
        if (!ByteCodeGenerator::IsFalse(cond) ||
            byteCodeGenerator->IsInDebugMode())
        {
            EmitBooleanExpression(cond, loopEntrance, continuePastLoop, byteCodeGenerator, funcInfo);
        }
        funcInfo->ReleaseLoc(cond);
    }
    else
    {
        if (cond)
        {
            if (!(cond->nop == knopInt &&
                cond->sxInt.lw != 0))
            {
                Js::ByteCodeLabel trueLabel = byteCodeGenerator->Writer()->DefineLabel();
                EmitBooleanExpression(cond, trueLabel, continuePastLoop, byteCodeGenerator, funcInfo);
                byteCodeGenerator->Writer()->MarkLabel(trueLabel);
            }
            funcInfo->ReleaseLoc(cond);
        }
        Emit(body, byteCodeGenerator, funcInfo, fReturnValue);
        funcInfo->ReleaseLoc(body);

        if (byteCodeGenerator->IsES6ForLoopSemanticsEnabled() &&
            forLoopBlock != nullptr)
        {
            CloneEmitBlock(forLoopBlock, byteCodeGenerator, funcInfo);
        }

        if (loopNode->emitLabels)
        {
            byteCodeGenerator->Writer()->MarkLabel(loopNode->sxStmt.continueLabel);
        }

        if (incr != nullptr)
        {
            Emit(incr, byteCodeGenerator, funcInfo, false);
            funcInfo->ReleaseLoc(incr);
        }

        byteCodeGenerator->Writer()->Br(loopEntrance);
    }

    byteCodeGenerator->Writer()->MarkLabel(continuePastLoop);
    if (loopNode->emitLabels)
    {
        byteCodeGenerator->Writer()->MarkLabel(loopNode->sxStmt.breakLabel);
    }

    byteCodeGenerator->Writer()->ExitLoop(loopId);
}

void ByteCodeGenerator::EmitInvertedLoop(ParseNode* outerLoop, ParseNode* invertedLoop, FuncInfo* funcInfo)
{
    Js::ByteCodeLabel invertedLoopLabel = this->m_writer.DefineLabel();
    Js::ByteCodeLabel afterInvertedLoop = this->m_writer.DefineLabel();

    // emit branch around original
    Emit(outerLoop->sxFor.pnodeInit, this, funcInfo, false);
    funcInfo->ReleaseLoc(outerLoop->sxFor.pnodeInit);
    this->m_writer.BrS(Js::OpCode::BrNotHasSideEffects, invertedLoopLabel, Js::SideEffects_Any);

    // emit original
    EmitLoop(outerLoop, outerLoop->sxFor.pnodeCond, outerLoop->sxFor.pnodeBody,
        outerLoop->sxFor.pnodeIncr, this, funcInfo, false);

    // clear temporary registers since inverted loop may share nodes with
    // emitted original loop
    VisitClearTmpRegs(outerLoop, this, funcInfo);

    // emit branch around inverted
    this->m_writer.Br(afterInvertedLoop);
    this->m_writer.MarkLabel(invertedLoopLabel);

    // Emit a zero trip test for the original outer-loop
    Js::ByteCodeLabel zeroTrip = this->m_writer.DefineLabel();
    ParseNode* testNode = this->GetParser()->CopyPnode(outerLoop->sxFor.pnodeCond);
    EmitBooleanExpression(testNode, zeroTrip, afterInvertedLoop, this, funcInfo);
    this->m_writer.MarkLabel(zeroTrip);
    funcInfo->ReleaseLoc(testNode);

    // emit inverted
    Emit(invertedLoop->sxFor.pnodeInit, this, funcInfo, false);
    funcInfo->ReleaseLoc(invertedLoop->sxFor.pnodeInit);
    EmitLoop(invertedLoop, invertedLoop->sxFor.pnodeCond, invertedLoop->sxFor.pnodeBody,
        invertedLoop->sxFor.pnodeIncr, this, funcInfo, false);
    this->m_writer.MarkLabel(afterInvertedLoop);
}

void EmitGetIterator(Js::RegSlot iteratorLocation, Js::RegSlot iterableLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{
    // get iterator object from the iterable
    EmitInvoke(iteratorLocation, iterableLocation, Js::PropertyIds::_symbolIterator, byteCodeGenerator, funcInfo);

    // throw TypeError if the result is not an object
    Js::ByteCodeLabel skipThrow = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrOnObject_A, skipThrow, iteratorLocation);
    byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(JSERR_NeedObject));
    byteCodeGenerator->Writer()->MarkLabel(skipThrow);
}

void EmitIteratorNext(Js::RegSlot itemLocation, Js::RegSlot iteratorLocation, Js::RegSlot nextInputLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{
    // invoke next() on the iterator
    if (nextInputLocation == Js::Constants::NoRegister)
    {
        EmitInvoke(itemLocation, iteratorLocation, Js::PropertyIds::next, byteCodeGenerator, funcInfo);
    }
    else
    {
        EmitInvoke(itemLocation, iteratorLocation, Js::PropertyIds::next, byteCodeGenerator, funcInfo, nextInputLocation);
    }

    // throw TypeError if the result is not an object
    Js::ByteCodeLabel skipThrow = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrOnObject_A, skipThrow, itemLocation);
    byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(JSERR_NeedObject));
    byteCodeGenerator->Writer()->MarkLabel(skipThrow);
}

void EmitIteratorComplete(Js::RegSlot doneLocation, Js::RegSlot iteratorResultLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{
    // get the iterator result's "done" property
    uint cacheId = funcInfo->FindOrAddInlineCacheId(iteratorResultLocation, Js::PropertyIds::done, false, false);
    byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, doneLocation, iteratorResultLocation, cacheId);

    // Do not need to do ToBoolean explicitly with current uses of EmitIteratorComplete since BrTrue_A does this.
    // Add a ToBoolean controlled by template flag if needed for new uses later on.
}

void EmitIteratorValue(Js::RegSlot valueLocation, Js::RegSlot iteratorResultLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{
    // get the iterator result's "value" property
    uint cacheId = funcInfo->FindOrAddInlineCacheId(iteratorResultLocation, Js::PropertyIds::value, false, false);
    byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, valueLocation, iteratorResultLocation, cacheId);
}

void EmitForInOrForOf(ParseNode *loopNode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, BOOL fReturnValue)
{
    bool isForIn = (loopNode->nop == knopForIn);
    Assert(isForIn || loopNode->nop == knopForOf);

    BeginEmitBlock(loopNode->sxForInOrForOf.pnodeBlock, byteCodeGenerator, funcInfo);

    byteCodeGenerator->StartStatement(loopNode);
    funcInfo->AcquireLoc(loopNode);

    // Record the branch bytecode offset.
    // This is used for "ignore exception" and "set next stmt" scenarios. See ProbeContainer::GetNextUserStatementOffsetForAdvance:
    // If there is a branch recorded between current offset and next stmt offset, we'll use offset of the branch recorded,
    // otherwise use offset of next stmt.
    // The idea here is that when we bail out after ignore exception, we need to bail out to the beginning of the ForIn,
    // but currently ForIn stmt starts at the condition part, which is needed for correct handling of break point on ForIn
    // (break every time on the loop back edge) and correct display of current statement under debugger.
    // See WinBlue 231880 for details.
    byteCodeGenerator->Writer()->RecordStatementAdjustment(Js::FunctionBody::SAT_All);
    if (byteCodeGenerator->IsES6ForLoopSemanticsEnabled() &&
        loopNode->sxForInOrForOf.pnodeBlock->sxBlock.HasBlockScopedContent())
    {
        byteCodeGenerator->Writer()->RecordForInOrOfCollectionScope();
    }
    Js::ByteCodeLabel loopEntrance = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel continuePastLoop = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel skipPastLoop = byteCodeGenerator->Writer()->DefineLabel();

    if (loopNode->sxForInOrForOf.pnodeLval->nop == knopVarDecl)
    {
        EmitReference(loopNode->sxForInOrForOf.pnodeLval, byteCodeGenerator, funcInfo);
    }

    Emit(loopNode->sxForInOrForOf.pnodeObj, byteCodeGenerator, funcInfo, false); // evaluate collection expression
    funcInfo->ReleaseLoc(loopNode->sxForInOrForOf.pnodeObj);

    if (byteCodeGenerator->IsES6ForLoopSemanticsEnabled())
    {
        EndEmitBlock(loopNode->sxForInOrForOf.pnodeBlock, byteCodeGenerator, funcInfo);
        if (loopNode->sxForInOrForOf.pnodeBlock->sxBlock.scope != nullptr)
        {
            loopNode->sxForInOrForOf.pnodeBlock->sxBlock.scope->ForEachSymbol([](Symbol *sym) {
                sym->SetIsTrackedForDebugger(false);
            });
        }
    }

    // Grab registers for the enumerator and for the current enumerated item.
    // The enumerator register will be released after this call returns.
    loopNode->sxForInOrForOf.itemLocation = funcInfo->AcquireTmpRegister();

    if (isForIn)
    {
        // get enumerator from the collection
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::GetForInEnumerator, loopNode->location, loopNode->sxForInOrForOf.pnodeObj->location);
    }
    else
    {
        // We want call profile information on the @@iterator call, so instead of adding a GetForOfIterator bytecode op
        // to do all the following work in a helper do it explicitly in bytecode so that the @@iterator call is exposed
        // to the profiler and JIT.

        // If collection is null or undefined, don't enter the loop.
        byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrSrEq_A, skipPastLoop, loopNode->sxForInOrForOf.pnodeObj->location, funcInfo->nullConstantRegister);
        byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrSrEq_A, skipPastLoop, loopNode->sxForInOrForOf.pnodeObj->location, funcInfo->undefinedConstantRegister);

        // do a ToObject on the collection
        Js::RegSlot tmpObj = funcInfo->AcquireTmpRegister();
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Conv_Obj, tmpObj, loopNode->sxForInOrForOf.pnodeObj->location);

        EmitGetIterator(loopNode->location, tmpObj, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseTmpRegister(tmpObj);
    }
    byteCodeGenerator->EndStatement(loopNode);

    // Need to increment loop count whether we are going into profile or not for HasLoop()
    uint loopId = byteCodeGenerator->Writer()->EnterLoop(loopEntrance);
    loopNode->sxForInOrForOf.loopId = loopId;

    byteCodeGenerator->StartStatement(loopNode->sxForInOrForOf.pnodeLval);

    if (isForIn)
    {
        // branch past loop when GetCurrentAndMoveNext returns nullptr
        byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrOnEmpty, continuePastLoop, loopNode->sxForInOrForOf.itemLocation, loopNode->location);
    }
    else
    {
        EmitIteratorNext(loopNode->sxForInOrForOf.itemLocation, loopNode->location, Js::Constants::NoRegister, byteCodeGenerator, funcInfo);

        Js::RegSlot doneLocation = funcInfo->AcquireTmpRegister();
        EmitIteratorComplete(doneLocation, loopNode->sxForInOrForOf.itemLocation, byteCodeGenerator, funcInfo);

        // branch past loop if the result's done property is truthy
        byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, continuePastLoop, doneLocation);
        funcInfo->ReleaseTmpRegister(doneLocation);

        // otherwise put result's value property in itemLocation
        EmitIteratorValue(loopNode->sxForInOrForOf.itemLocation, loopNode->sxForInOrForOf.itemLocation, byteCodeGenerator, funcInfo);
    }

    if (loopNode->sxForInOrForOf.pnodeLval->nop != knopVarDecl &&
        loopNode->sxForInOrForOf.pnodeLval->nop != knopLetDecl)
    {
        EmitReference(loopNode->sxForInOrForOf.pnodeLval, byteCodeGenerator, funcInfo);
    }
    else
    {
        Symbol * sym = loopNode->sxForInOrForOf.pnodeLval->sxVar.sym;
        sym->SetNeedDeclaration(false);
    }

    if (byteCodeGenerator->IsES6ForLoopSemanticsEnabled())
    {
        BeginEmitBlock(loopNode->sxForInOrForOf.pnodeBlock, byteCodeGenerator, funcInfo);
    }

    EmitAssignment(nullptr, loopNode->sxForInOrForOf.pnodeLval, loopNode->sxForInOrForOf.itemLocation, byteCodeGenerator, funcInfo);

    byteCodeGenerator->EndStatement(loopNode->sxForInOrForOf.pnodeLval);

    funcInfo->ReleaseReference(loopNode->sxForInOrForOf.pnodeLval);

    Emit(loopNode->sxForInOrForOf.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);
    funcInfo->ReleaseLoc(loopNode->sxForInOrForOf.pnodeBody);

    if (byteCodeGenerator->IsES6ForLoopSemanticsEnabled())
    {
        EndEmitBlock(loopNode->sxForInOrForOf.pnodeBlock, byteCodeGenerator, funcInfo);
    }

    funcInfo->ReleaseTmpRegister(loopNode->sxForInOrForOf.itemLocation);
    if (loopNode->emitLabels)
    {
        byteCodeGenerator->Writer()->MarkLabel(loopNode->sxForInOrForOf.continueLabel);
    }
    byteCodeGenerator->Writer()->Br(loopEntrance);
    byteCodeGenerator->Writer()->MarkLabel(continuePastLoop);
    if (loopNode->emitLabels)
    {
        byteCodeGenerator->Writer()->MarkLabel(loopNode->sxForInOrForOf.breakLabel);
    }
    byteCodeGenerator->Writer()->ExitLoop(loopId);

    if (isForIn)
    {
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::ReleaseForInEnumerator, loopNode->location);
    }
    else
    {
        byteCodeGenerator->Writer()->MarkLabel(skipPastLoop);
    }

    if (!byteCodeGenerator->IsES6ForLoopSemanticsEnabled())
    {
        EndEmitBlock(loopNode->sxForInOrForOf.pnodeBlock, byteCodeGenerator, funcInfo);
    }
}

void EmitArrayLiteral(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    funcInfo->AcquireLoc(pnode);
    ParseNode *args = pnode->sxUni.pnode1;
    if (args == nullptr)
    {
        byteCodeGenerator->Writer()->Reg1Unsigned1(
            pnode->sxArrLit.hasMissingValues ? Js::OpCode::NewScArrayWithMissingValues : Js::OpCode::NewScArray,
            pnode->location,
            ByteCodeGenerator::DefaultArraySize);
    }
    else
    {
        SetNewArrayElements(pnode, pnode->location, byteCodeGenerator, funcInfo);
    }
}

void EmitJumpCleanup(ParseNode *pnode, ParseNode *pnodeTarget, ByteCodeGenerator *byteCodeGenerator, FuncInfo * funcInfo)
{
    for (; pnode != pnodeTarget; pnode = pnode->sxStmt.pnodeOuter)
    {
        switch (pnode->nop)
        {
        case knopTry:
        case knopCatch:
        case knopFinally:
            // We insert OpCode::Leave when there is a 'return' inside try/catch/finally.
            // This is for flow control and does not participate in identifying boundaries of try/catch blocks,
            // thus we shouldn't call RecordCrossFrameEntryExitRecord() here.
            byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
            break;

#if ENABLE_PROFILE_INFO
        case knopWhile:
        case knopDoWhile:
        case knopFor:
        case knopForIn:
        case knopForOf:
            if (Js::DynamicProfileInfo::EnableImplicitCallFlags(funcInfo->GetParsedFunctionBody()))
            {
                byteCodeGenerator->Writer()->Unsigned1(Js::OpCode::ProfiledLoopEnd, pnode->sxLoop.loopId);
            }
#endif
        }
    }
}

void EmitBinaryOpnds(ParseNode *pnode1, ParseNode *pnode2, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    // If opnd2 can overwrite opnd1, make sure the value of opnd1 is stashed away.
    if (MayHaveSideEffectOnNode(pnode1, pnode2))
    {
        SaveOpndValue(pnode1, funcInfo);
    }

    Emit(pnode1, byteCodeGenerator, funcInfo, false);

    if (pnode1->nop == knopComputedName && pnode2->nop == knopClassDecl &&
        (pnode2->sxClass.pnodeConstructor == nullptr || pnode2->sxClass.pnodeConstructor->nop != knopVarDecl))
    {
        Emit(pnode2, byteCodeGenerator, funcInfo, false, false, pnode1);
    }
    else
    {
        Emit(pnode2, byteCodeGenerator, funcInfo, false);
    }
}

void EmitBinaryReference(ParseNode *pnode1, ParseNode *pnode2, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, BOOL fLoadLhs)
{
    // Make sure that the RHS of an assignment doesn't kill the opnd's of the expression on the LHS.
    switch (pnode1->nop)
    {
    case knopName:
        if (fLoadLhs && MayHaveSideEffectOnNode(pnode1, pnode2))
        {
            // Given x op y, y may kill x, so stash x.
            // Note that this only matters if we're loading x prior to the op.
            SaveOpndValue(pnode1, funcInfo);
        }
        break;
    case knopDot:
        if (fLoadLhs)
        {
            // We're loading the value of the LHS before the RHS, so make sure the LHS gets a register first.
            funcInfo->AcquireLoc(pnode1);
        }
        if (MayHaveSideEffectOnNode(pnode1->sxBin.pnode1, pnode2))
        {
            // Given x.y op z, z may kill x, so stash x away.
            SaveOpndValue(pnode1->sxBin.pnode1, funcInfo);
        }
        break;
    case knopIndex:
        if (fLoadLhs)
        {
            // We're loading the value of the LHS before the RHS, so make sure the LHS gets a register first.
            funcInfo->AcquireLoc(pnode1);
        }
        if (MayHaveSideEffectOnNode(pnode1->sxBin.pnode1, pnode2) ||
            MayHaveSideEffectOnNode(pnode1->sxBin.pnode1, pnode1->sxBin.pnode2))
        {
            // Given x[y] op z, y or z may kill x, so stash x away.
            SaveOpndValue(pnode1->sxBin.pnode1, funcInfo);
        }
        if (MayHaveSideEffectOnNode(pnode1->sxBin.pnode2, pnode2))
        {
            // Given x[y] op z, z may kill y, so stash y away.
            // But make sure that x gets a register before y.
            funcInfo->AcquireLoc(pnode1->sxBin.pnode1);
            SaveOpndValue(pnode1->sxBin.pnode2, funcInfo);
        }
        break;
    }

    if (fLoadLhs)
    {
        // Emit code to load the value of the LHS.
        EmitLoad(pnode1, byteCodeGenerator, funcInfo);
    }
    else
    {
        // Emit code to evaluate the LHS opnds, but don't load the LHS's value.
        EmitReference(pnode1, byteCodeGenerator, funcInfo);
    }

    // Evaluate the RHS.
    Emit(pnode2, byteCodeGenerator, funcInfo, false);
}

void EmitUseBeforeDeclarationRuntimeError(ByteCodeGenerator * byteCodeGenerator, Js::RegSlot location, bool fLoadUndef)
{
    byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_UseBeforeDeclaration));
    // Load something into register in order to do not confuse IRBuilder. This value will never be used.
    if (fLoadUndef)
    {
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdUndef, location);
    }
}

bool EmitUseBeforeDeclaration(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    Symbol *sym = nullptr;
    bool fAcquireLoc = true;
    if (pnode->nop == knopName)
    {
        sym = pnode->sxPid.sym;
    }
    else if (pnode->nop == knopVarDecl)
    {
        fAcquireLoc = false;
        sym = pnode->sxVar.sym;
    }
    else if ((ParseNode::Grfnop(pnode->nop) & fnopAsg) != 0)
    {
        if ((ParseNode::Grfnop(pnode->nop) & fnopBin) != 0 && pnode->sxBin.pnode1->nop == knopName)
        {
            sym = pnode->sxBin.pnode1->sxPid.sym;
        }
        if ((ParseNode::Grfnop(pnode->nop) & fnopUni) != 0 && pnode->sxUni.pnode1->nop == knopName)
        {
            sym = pnode->sxUni.pnode1->sxPid.sym;
        }
    }

    // Don't emit static use-before-declaration error in a closure or dynamic scope case. We detect such cases with dynamic checks,
    // if necessary.
    if (sym != nullptr && sym->GetNeedDeclaration() && byteCodeGenerator->GetCurrentScope()->HasStaticPathToAncestor(sym->GetScope()) && sym->GetScope()->GetFunc() == funcInfo)
    {
        if (fAcquireLoc)
        {
            funcInfo->AcquireLoc(pnode);
        }
        EmitUseBeforeDeclarationRuntimeError(byteCodeGenerator, pnode->location, fAcquireLoc);
        return true;
    }

    return false;
}

void EmitBinary(Js::OpCode opcode, ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    byteCodeGenerator->StartStatement(pnode);
    EmitBinaryOpnds(pnode->sxBin.pnode1, pnode->sxBin.pnode2, byteCodeGenerator, funcInfo);
    funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
    funcInfo->ReleaseLoc(pnode->sxBin.pnode1);
    funcInfo->AcquireLoc(pnode);
    byteCodeGenerator->Writer()->Reg3(opcode,
        pnode->location,
        pnode->sxBin.pnode1->location,
        pnode->sxBin.pnode2->location);
    byteCodeGenerator->EndStatement(pnode);
}

bool CollectConcat(ParseNode *pnodeAdd, DListCounted<ParseNode *, ArenaAllocator>& concatOpnds, ArenaAllocator *arenaAllocator)
{
    Assert(pnodeAdd->nop == knopAdd);
    Assert(pnodeAdd->CanFlattenConcatExpr());

    bool doConcatString = false;
    DList<ParseNode*, ArenaAllocator> pnodeStack(arenaAllocator);
    pnodeStack.Prepend(pnodeAdd->sxBin.pnode2);
    ParseNode * pnode = pnodeAdd->sxBin.pnode1;
    while (true)
    {
        if (!pnode->CanFlattenConcatExpr())
        {
            concatOpnds.Append(pnode);
        }
        else if (pnode->nop == knopStr)
        {
            concatOpnds.Append(pnode);

            // Detect if there are any string larger then the append size limit.
            // If there are, we can do concat; otherwise, still use add so we will not lose the AddLeftDead opportunities.
            doConcatString = doConcatString || !Js::CompoundString::ShouldAppendChars(pnode->sxPid.pid->Cch());
        }
        else
        {
            Assert(pnode->nop == knopAdd);
            pnodeStack.Prepend(pnode->sxBin.pnode2);
            pnode = pnode->sxBin.pnode1;
            continue;
        }

        if (pnodeStack.Empty())
        {
            break;
        }

        pnode = pnodeStack.Head();
        pnodeStack.RemoveHead();
    }

    return doConcatString;
}

void EmitConcat3(ParseNode *pnode, ParseNode *pnode1, ParseNode *pnode2, ParseNode *pnode3, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    byteCodeGenerator->StartStatement(pnode);
    if (MayHaveSideEffectOnNode(pnode1, pnode2) || MayHaveSideEffectOnNode(pnode1, pnode3))
    {
        SaveOpndValue(pnode1, funcInfo);
    }

    if (MayHaveSideEffectOnNode(pnode2, pnode3))
    {
        SaveOpndValue(pnode2, funcInfo);
    }

    Emit(pnode1, byteCodeGenerator, funcInfo, false);
    Emit(pnode2, byteCodeGenerator, funcInfo, false);
    Emit(pnode3, byteCodeGenerator, funcInfo, false);
    funcInfo->ReleaseLoc(pnode3);
    funcInfo->ReleaseLoc(pnode2);
    funcInfo->ReleaseLoc(pnode1);
    funcInfo->AcquireLoc(pnode);
    byteCodeGenerator->Writer()->Reg4(Js::OpCode::Concat3,
        pnode->location,
        pnode1->location,
        pnode2->location,
        pnode3->location);
    byteCodeGenerator->EndStatement(pnode);
}

void EmitNewConcatStrMulti(ParseNode *pnode, uint8 count, ParseNode *pnode1, ParseNode *pnode2, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    EmitBinaryOpnds(pnode1, pnode2, byteCodeGenerator, funcInfo);
    funcInfo->ReleaseLoc(pnode2);
    funcInfo->ReleaseLoc(pnode1);
    funcInfo->AcquireLoc(pnode);
    byteCodeGenerator->Writer()->Reg3B1(Js::OpCode::NewConcatStrMulti,
        pnode->location,
        pnode1->location,
        pnode2->location,
        count);
}

void EmitAdd(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    Assert(pnode->nop == knopAdd);

    if (pnode->CanFlattenConcatExpr())
    {
        // We should only have a string concat if the feature is on.
        Assert(!PHASE_OFF1(Js::ByteCodeConcatExprOptPhase));
        DListCounted<ParseNode*, ArenaAllocator> concatOpnds(byteCodeGenerator->GetAllocator());
        bool doConcatString = CollectConcat(pnode, concatOpnds, byteCodeGenerator->GetAllocator());
        if (doConcatString)
        {
            uint concatCount = concatOpnds.Count();
            Assert(concatCount >= 2);

            // Don't do concatN if the number is too high
            // CONSIDER: although we could have done multiple ConcatNs
            if (concatCount > 2 && concatCount <= UINT8_MAX)
            {
#if DBG
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
                ParseNode * pnode1 = concatOpnds.Head();
                concatOpnds.RemoveHead();
                ParseNode * pnode2 = concatOpnds.Head();
                concatOpnds.RemoveHead();
                if (concatCount == 3)
                {
                    OUTPUT_TRACE_DEBUGONLY(Js::ByteCodeConcatExprOptPhase, L"%s(%s) offset:#%d : Concat3\n",
                        funcInfo->GetParsedFunctionBody()->GetDisplayName(),
                        funcInfo->GetParsedFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                        byteCodeGenerator->Writer()->ByteCodeDataSize());
                    EmitConcat3(pnode, pnode1, pnode2, concatOpnds.Head(), byteCodeGenerator, funcInfo);
                    return;
                }

                OUTPUT_TRACE_DEBUGONLY(Js::ByteCodeConcatExprOptPhase, L"%s(%s) offset:#%d: ConcatMulti %d\n",
                    funcInfo->GetParsedFunctionBody()->GetDisplayName(),
                    funcInfo->GetParsedFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                    byteCodeGenerator->Writer()->ByteCodeDataSize(), concatCount);
                byteCodeGenerator->StartStatement(pnode);
                funcInfo->AcquireLoc(pnode);

                // CONSIDER: this may cause the backend not able CSE repeating pattern within the concat.
                EmitNewConcatStrMulti(pnode, (uint8)concatCount, pnode1, pnode2, byteCodeGenerator, funcInfo);

                uint i = 2;
                do
                {
                    ParseNode * currNode = concatOpnds.Head();
                    concatOpnds.RemoveHead();
                    ParseNode * currNode2 = concatOpnds.Head();
                    concatOpnds.RemoveHead();

                    EmitBinaryOpnds(currNode, currNode2, byteCodeGenerator, funcInfo);
                    funcInfo->ReleaseLoc(currNode2);
                    funcInfo->ReleaseLoc(currNode);
                    byteCodeGenerator->Writer()->Reg3B1(
                        Js::OpCode::SetConcatStrMultiItem2, pnode->location, currNode->location, currNode2->location, (uint8)i);
                    i += 2;
                }
                while (concatOpnds.Count() > 1);

                if (!concatOpnds.Empty())
                {
                    ParseNode * currNode = concatOpnds.Head();
                    Emit(currNode, byteCodeGenerator, funcInfo, false);
                    funcInfo->ReleaseLoc(currNode);
                    byteCodeGenerator->Writer()->Reg2B1(
                        Js::OpCode::SetConcatStrMultiItem, pnode->location, currNode->location, (uint8)i);
                    i++;
                }

                Assert(concatCount == i);
                byteCodeGenerator->EndStatement(pnode);
                return;
            }
        }

        // Since we collected all the node already, let's just emit them instead of doing it recursively.
        byteCodeGenerator->StartStatement(pnode);
        ParseNode * currNode = concatOpnds.Head();
        concatOpnds.RemoveHead();
        ParseNode * currNode2 = concatOpnds.Head();
        concatOpnds.RemoveHead();

        EmitBinaryOpnds(currNode, currNode2, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(currNode2);
        funcInfo->ReleaseLoc(currNode);
        Js::RegSlot dstReg = funcInfo->AcquireLoc(pnode);
        byteCodeGenerator->Writer()->Reg3(
            Js::OpCode::Add_A, dstReg, currNode->location, currNode2->location);
        while (!concatOpnds.Empty())
        {
            currNode = concatOpnds.Head();
            concatOpnds.RemoveHead();
            Emit(currNode, byteCodeGenerator, funcInfo, false);
            funcInfo->ReleaseLoc(currNode);
            byteCodeGenerator->Writer()->Reg3(
                Js::OpCode::Add_A, dstReg, dstReg, currNode->location);
        }
        byteCodeGenerator->EndStatement(pnode);
    }
    else
    {
        EmitBinary(Js::OpCode::Add_A, pnode, byteCodeGenerator, funcInfo);
    }
}

void EmitSuperFieldPatch(FuncInfo* funcInfo, ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator)
{
    ParseNodePtr propFuncNode = funcInfo->root;

    if (byteCodeGenerator->GetFlags() & fscrEval)
    {
        // If we are inside an eval, ScopedLdSuper will take care of the patch.
        return;
    }

    if (funcInfo->IsLambda())
    {
        FuncInfo *parent = byteCodeGenerator->FindEnclosingNonLambda();
        propFuncNode = parent->root;
    }

    // No need to emit a LdFld for the constructor.
    if (propFuncNode->sxFnc.IsClassConstructor())
    {
        return;
    }

    if (!propFuncNode->sxFnc.IsClassMember() || propFuncNode->sxFnc.pid == nullptr)
    {
        // Non-methods will fail lookup.
        return;
    }
    if (propFuncNode->sxFnc.pid->GetPropertyId() == Js::Constants::NoProperty)
    {
        byteCodeGenerator->AssignPropertyId(propFuncNode->sxFnc.pid);
    }

    // Load the current method's property ID from super instead of using super directly.
    Js::RegSlot superLoc = funcInfo->superRegister;
    pnode->sxCall.pnodeTarget->location = Js::Constants::NoRegister;
    Js::RegSlot superPropLoc = funcInfo->AcquireLoc(pnode->sxCall.pnodeTarget);
    Js::PropertyId propertyId = propFuncNode->sxFnc.pid->GetPropertyId();
    uint cacheId = funcInfo->FindOrAddInlineCacheId(superLoc, propertyId, true, false);
    byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdMethodFld, superPropLoc, superLoc, cacheId);

    propFuncNode->sxFnc.pnodeName = nullptr;
}

struct ByteCodeGenerator::TryScopeRecord : public JsUtil::DoublyLinkedListElement<TryScopeRecord>
{
    Js::OpCode op;
    Js::ByteCodeLabel label;
    Js::RegSlot reg1;
    Js::RegSlot reg2;

    TryScopeRecord(Js::OpCode op, Js::ByteCodeLabel label) : op(op), label(label), reg1(Js::Constants::NoRegister), reg2(Js::Constants::NoRegister) { }
    TryScopeRecord(Js::OpCode op, Js::ByteCodeLabel label, Js::RegSlot r1, Js::RegSlot r2) : op(op), label(label), reg1(r1), reg2(r2) { }
};

void ByteCodeGenerator::EmitLeaveOpCodesBeforeYield()
{
    for (TryScopeRecord* node = this->tryScopeRecordsList.Tail(); node != nullptr; node = node->Previous())
    {
        switch (node->op)
        {
        case Js::OpCode::TryFinallyWithYield:
            this->Writer()->Empty(Js::OpCode::LeaveNull);
            break;
        case Js::OpCode::TryCatch:
        case Js::OpCode::ResumeFinally:
        case Js::OpCode::ResumeCatch:
            this->Writer()->Empty(Js::OpCode::Leave);
            break;
        default:
            AssertMsg(false, "Unexpected OpCode before Yield in the Try-Catch-Finally cache for generator!");
            break;
        }
    }
}

void ByteCodeGenerator::EmitTryBlockHeadersAfterYield()
{
    for (TryScopeRecord* node = this->tryScopeRecordsList.Head(); node != nullptr; node = node->Next())
    {
        switch (node->op)
        {
        case Js::OpCode::TryCatch:
            this->Writer()->Br(node->op, node->label);
            break;
        case Js::OpCode::TryFinallyWithYield:
        case Js::OpCode::ResumeFinally:
            this->Writer()->BrReg2(node->op, node->label, node->reg1, node->reg2);
            break;
        case Js::OpCode::ResumeCatch:
            this->Writer()->Empty(node->op);
            break;
        default:
            AssertMsg(false, "Unexpected OpCode after yield in the Try-Catch-Finally cache for generator!");
            break;
        }
    }
}

void EmitYield(Js::RegSlot inputLocation, Js::RegSlot resultLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo,
    Js::RegSlot yieldStarIterator = Js::Constants::NoRegister)
{
    byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, funcInfo->yieldRegister, inputLocation);

    byteCodeGenerator->EmitLeaveOpCodesBeforeYield();
    byteCodeGenerator->Writer()->Reg2(Js::OpCode::Yield, funcInfo->yieldRegister, funcInfo->yieldRegister);
    byteCodeGenerator->EmitTryBlockHeadersAfterYield();

    if (yieldStarIterator == Js::Constants::NoRegister)
    {
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::ResumeYield, resultLocation, funcInfo->yieldRegister);
    }
    else
    {
        byteCodeGenerator->Writer()->Reg3(Js::OpCode::ResumeYieldStar, resultLocation, funcInfo->yieldRegister, yieldStarIterator);
    }
}

void EmitYieldStar(ParseNode* yieldStarNode, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{
    funcInfo->AcquireLoc(yieldStarNode);

    Js::ByteCodeLabel loopEntrance = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel continuePastLoop = byteCodeGenerator->Writer()->DefineLabel();

    Js::RegSlot iteratorLocation = funcInfo->AcquireTmpRegister();

    // Evaluate operand
    Emit(yieldStarNode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
    funcInfo->ReleaseLoc(yieldStarNode->sxUni.pnode1);

    EmitGetIterator(iteratorLocation, yieldStarNode->sxUni.pnode1->location, byteCodeGenerator, funcInfo);

    uint loopId = byteCodeGenerator->Writer()->EnterLoop(loopEntrance);
    // since a yield* doesn't have a user defined body, we cannot return from this loop
    // which means we don't need to support EmitJumpCleanup() and there do not need to
    // remember the loopId like the loop statements do.

    // Get a temporary to hold the input for the next() calls.  Initialize it to undefined for the first call.
    Js::RegSlot nextInputLocation = funcInfo->AcquireTmpRegister();
    byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, nextInputLocation, funcInfo->undefinedConstantRegister);

    // Call the iterator's next()
    EmitIteratorNext(yieldStarNode->location, iteratorLocation, nextInputLocation, byteCodeGenerator, funcInfo);

    Js::RegSlot doneLocation = funcInfo->AcquireTmpRegister();
    EmitIteratorComplete(doneLocation, yieldStarNode->location, byteCodeGenerator, funcInfo);

    // Put the iterator result's value in yieldStarNode->location regardless of the done property value.  The value will
    // be used as the result value of the yield* operator expression in the case when done is true.
    EmitIteratorValue(yieldStarNode->location, yieldStarNode->location, byteCodeGenerator, funcInfo);

    // branch past the loop if the done property is truthy
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, continuePastLoop, doneLocation);
    funcInfo->ReleaseTmpRegister(doneLocation);

    EmitYield(yieldStarNode->location, nextInputLocation, byteCodeGenerator, funcInfo, iteratorLocation);

    funcInfo->ReleaseTmpRegister(nextInputLocation);
    funcInfo->ReleaseTmpRegister(iteratorLocation);

    byteCodeGenerator->Writer()->Br(loopEntrance);
    byteCodeGenerator->Writer()->MarkLabel(continuePastLoop);
    byteCodeGenerator->Writer()->ExitLoop(loopId);
}

void TrackIntConstantsOnGlobalUserObject(ByteCodeGenerator *byteCodeGenerator, bool isSymGlobalAndSingleAssignment, Js::PropertyId propertyId)
{
    if (isSymGlobalAndSingleAssignment)
    {
        byteCodeGenerator->GetScriptContext()->TrackIntConstPropertyOnGlobalUserObject(propertyId);
    }
}

void TrackIntConstantsOnGlobalObject(ByteCodeGenerator *byteCodeGenerator, bool isSymGlobalAndSingleAssignment, Js::PropertyId propertyId)
{
    if (isSymGlobalAndSingleAssignment)
    {
        byteCodeGenerator->GetScriptContext()->TrackIntConstPropertyOnGlobalObject(propertyId);
    }
}

void TrackIntConstantsOnGlobalObject(ByteCodeGenerator *byteCodeGenerator, Symbol *sym)
{
    if (sym && sym->GetIsGlobal() && sym->IsAssignedOnce())
    {
        Js::PropertyId propertyId = sym->EnsurePosition(byteCodeGenerator);
        byteCodeGenerator->GetScriptContext()->TrackIntConstPropertyOnGlobalObject(propertyId);
    }
}

void TrackMemberNodesInObjectForIntConstants(ByteCodeGenerator *byteCodeGenerator, ParseNodePtr objNode)
{
    Assert(objNode->nop == knopObject);
    Assert(ParseNode::Grfnop(objNode->nop) & fnopUni);

    ParseNodePtr memberList = objNode->sxUni.pnode1;

    if (memberList != nullptr)
    {
        // Iterate through all the member nodes
        while (memberList->nop == knopList)
        {
            ParseNodePtr memberNode = memberList->sxBin.pnode1;
            ParseNodePtr memberNameNode = memberNode->sxBin.pnode1;
            ParseNodePtr memberValNode = memberNode->sxBin.pnode2;

            if (memberNameNode->nop != knopComputedName && memberValNode->nop == knopInt)
            {
                Js::PropertyId propertyId = memberNameNode->sxPid.PropertyIdFromNameNode();
                TrackIntConstantsOnGlobalUserObject(byteCodeGenerator, true, propertyId);
            }
            memberList = memberList->sxBin.pnode2;
        }

        ParseNode *memberNameNode = memberList->sxBin.pnode1;
        ParseNode *memberValNode = memberList->sxBin.pnode2;

        if (memberNameNode->nop != knopComputedName && memberValNode->nop == knopInt)
        {
            Js::PropertyId propertyId = memberNameNode->sxPid.PropertyIdFromNameNode();
            TrackIntConstantsOnGlobalUserObject(byteCodeGenerator, true, propertyId);
        }
    }
}

void TrackGlobalIntAssignmentsForknopDotProps(ParseNodePtr knopDotNode, ByteCodeGenerator * byteCodeGenerator)
{
    Assert(knopDotNode->nop == knopDot);

    ParseNodePtr objectNode = knopDotNode->sxBin.pnode1;
    ParseNodePtr propertyNode = knopDotNode->sxBin.pnode2;
    bool isSymGlobalAndSingleAssignment = false;

    if (objectNode->nop == knopName)
    {
        Symbol * sym = objectNode->sxVar.sym;
        isSymGlobalAndSingleAssignment = sym && sym->GetIsGlobal() && sym->IsAssignedOnce() && propertyNode->sxPid.pid->IsSingleAssignment();
        Js::PropertyId propertyId = propertyNode->sxPid.PropertyIdFromNameNode();
        TrackIntConstantsOnGlobalUserObject(byteCodeGenerator, isSymGlobalAndSingleAssignment, propertyId);
    }
    else if (objectNode->nop == knopThis)
    {
        // Assume knopThis always refer to GlobalObject
        // Cases like "this.a = "
        isSymGlobalAndSingleAssignment = propertyNode->sxPid.pid->IsSingleAssignment();
        Js::PropertyId propertyId = propertyNode->sxPid.PropertyIdFromNameNode();
        TrackIntConstantsOnGlobalObject(byteCodeGenerator, isSymGlobalAndSingleAssignment, propertyId);
    }
}

void TrackGlobalIntAssignments(ParseNodePtr pnode, ByteCodeGenerator * byteCodeGenerator)
{
    // Track the Global Int Constant properties' assignments here.
    uint nodeType = ParseNode::Grfnop(pnode->nop);
    if (nodeType & fnopAsg)
    {
        if (nodeType & fnopBin)
        {
            ParseNodePtr lhs = pnode->sxBin.pnode1;
            ParseNodePtr rhs = pnode->sxBin.pnode2;

            Assert(lhs && rhs);

            // Don't track other than integers and objects with member nodes.
            if (rhs->nop == knopObject && (ParseNode::Grfnop(rhs->nop) & fnopUni))
            {
                TrackMemberNodesInObjectForIntConstants(byteCodeGenerator, rhs);
            }
            else if (rhs->nop != knopInt &&
                ((rhs->nop != knopLsh && rhs->nop != knopRsh) || (rhs->sxBin.pnode1->nop != knopInt || rhs->sxBin.pnode2->nop != knopInt)))
            {
                return;
            }

            if (lhs->nop == knopName)
            {
                // Handle "a = <Integer>" cases here
                Symbol * sym = lhs->sxVar.sym;
                TrackIntConstantsOnGlobalObject(byteCodeGenerator, sym);
            }
            else if (lhs->nop == knopDot && lhs->sxBin.pnode2->nop == knopName)
            {
                // Cases like "obj.a = <Integer>"
                TrackGlobalIntAssignmentsForknopDotProps(lhs, byteCodeGenerator);
            }
        }
        else if (nodeType & fnopUni)
        {
            ParseNodePtr lhs = pnode->sxUni.pnode1;

            if (lhs->nop == knopName)
            {
                // Cases like "a++"
                Symbol * sym = lhs->sxVar.sym;
                TrackIntConstantsOnGlobalObject(byteCodeGenerator, sym);
            }
            else if (lhs->nop == knopDot && lhs->sxBin.pnode2->nop == knopName)
            {
                // Cases like "obj.a++"
                TrackGlobalIntAssignmentsForknopDotProps(lhs, byteCodeGenerator);
            }
        }
    }
}

void Emit(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, BOOL fReturnValue, bool isConstructorCall, ParseNode * bindPnode)
{
    if (pnode == nullptr)
    {
        return;
    }

    if (EmitUseBeforeDeclaration(pnode, byteCodeGenerator, funcInfo))
    {
        if (fReturnValue && IsExpressionStatement(pnode, byteCodeGenerator->GetScriptContext()))
        {
            // If this statement may produce the global function's return value, make sure we load something to the
            // return register for the JIT.

            // fReturnValue implies global function, which implies that "return" is a parse error.
            Assert(funcInfo->IsGlobalFunction());
            Assert(pnode->nop != knopReturn);
            byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdUndef, ByteCodeGenerator::ReturnRegister);
        }

        return;
    }

    ThreadContext::ProbeCurrentStackNoDispose(Js::Constants::MinStackByteCodeVisitor, byteCodeGenerator->GetScriptContext());

    TrackGlobalIntAssignments(pnode, byteCodeGenerator);

    // printNop(pnode->nop);
    switch (pnode->nop)
    {
    case knopList:
        EmitList(pnode, byteCodeGenerator, funcInfo);
        break;
    case knopInt:
        // currently, these are loaded at the top
        break;
    // PTNODE(knopFlt        , "flt const"    ,None    ,Flt  ,fnopLeaf|fnopConst)
    case knopFlt:
        // currently, these are loaded at the top
        break;
    // PTNODE(knopStr        , "str const"    ,None    ,Pid  ,fnopLeaf|fnopConst)
    case knopStr:
        // TODO: protocol for combining string constants
        break;
    // PTNODE(knopRegExp     , "reg expr"    ,None    ,Pid  ,fnopLeaf|fnopConst)
    case knopRegExp:
        funcInfo->GetParsedFunctionBody()->SetLiteralRegex(pnode->sxPid.regexPatternIndex, pnode->sxPid.regexPattern);
        byteCodeGenerator->Writer()->Reg1Unsigned1(Js::OpCode::NewRegEx, funcInfo->AcquireLoc(pnode), pnode->sxPid.regexPatternIndex);
        break;          // PTNODE(knopThis       , "this"        ,None    ,None ,fnopLeaf)
    case knopThis:
        // enregistered
        // Try to load 'this' from a scope slot if we are in a derived class constructor with scope slots. Otherwise, this is a nop.
        byteCodeGenerator->EmitScopeSlotLoadThis(funcInfo, funcInfo->thisPointerRegister);
        break;
    // PTNODE(knopNewTarget      , "new.target"       ,None    , None        , fnopLeaf)
    case knopNewTarget:
        break;
    // PTNODE(knopSuper      , "super"       ,None    , None        , fnopLeaf)
    case knopSuper:
        if (!funcInfo->IsClassMember())
        {
            FuncInfo* nonLambdaFunc = funcInfo;
            if (funcInfo->IsLambda())
            {
                nonLambdaFunc = byteCodeGenerator->FindEnclosingNonLambda();
            }

            if (nonLambdaFunc->IsGlobalFunction())
            {
                if ((byteCodeGenerator->GetFlags() & fscrEval))
                {
                    byteCodeGenerator->Writer()->Reg1(isConstructorCall ? Js::OpCode::ScopedLdSuperCtor : Js::OpCode::ScopedLdSuper, funcInfo->AcquireLoc(pnode));
                }
                else
                {
                    byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_BadSuperReference));
                }
            }
            else
            {
                Js::ByteCodeLabel errLabel = byteCodeGenerator->Writer()->DefineLabel();
                Js::ByteCodeLabel skipLabel = byteCodeGenerator->Writer()->DefineLabel();
                byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrEq_A, errLabel, funcInfo->superRegister, funcInfo->undefinedConstantRegister);
                byteCodeGenerator->Writer()->Br(Js::OpCode::Br, skipLabel);
                byteCodeGenerator->Writer()->MarkLabel(errLabel);
                byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_BadSuperReference));
                byteCodeGenerator->Writer()->MarkLabel(skipLabel);
            }
        }
        break;
    // PTNODE(knopNull       , "null"        ,Null    ,None ,fnopLeaf)
    case knopNull:
        // enregistered
        break;
    // PTNODE(knopFalse      , "false"        ,False   ,None ,fnopLeaf)
    case knopFalse:
        // enregistered
        break;
    // PTNODE(knopTrue       , "true"        ,True    ,None ,fnopLeaf)
    case knopTrue:
        // enregistered
        break;
    // PTNODE(knopEmpty      , "empty"        ,Empty   ,None ,fnopLeaf)
    case knopEmpty:
        break;
        // Unary operators.
    // PTNODE(knopNot        , "~"            ,BitNot  ,Uni  ,fnopUni)
    case knopNot:
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        byteCodeGenerator->Writer()->Reg2(
            Js::OpCode::Not_A, funcInfo->AcquireLoc(pnode), pnode->sxUni.pnode1->location);
        break;
    // PTNODE(knopNeg        , "unary -"    ,Neg     ,Uni  ,fnopUni)
    case knopNeg:
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        funcInfo->AcquireLoc(pnode);
        byteCodeGenerator->Writer()->Reg2(
            Js::OpCode::Neg_A, pnode->location, pnode->sxUni.pnode1->location);
        break;
    // PTNODE(knopPos        , "unary +"    ,Pos     ,Uni  ,fnopUni)
    case knopPos:
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        byteCodeGenerator->Writer()->Reg2(
            Js::OpCode::Conv_Num, funcInfo->AcquireLoc(pnode), pnode->sxUni.pnode1->location);
        break;
    // PTNODE(knopLogNot     , "!"            ,LogNot  ,Uni  ,fnopUni)
    case knopLogNot:
    {
        Js::ByteCodeLabel doneLabel = byteCodeGenerator->Writer()->DefineLabel();
        // For boolean expressions that compute a result, we have to burn a register for the result
        // so that the back end can identify it cheaply as a single temp lifetime. Revisit this if we do
        // full-on renaming in the back end.
        funcInfo->AcquireLoc(pnode);
        if (pnode->sxUni.pnode1->nop == knopInt)
        {
            long value = pnode->sxUni.pnode1->sxInt.lw;
            Js::OpCode op = value ? Js::OpCode::LdFalse : Js::OpCode::LdTrue;
            byteCodeGenerator->Writer()->Reg1(op, pnode->location);
        }
        else
        {
            Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
            byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, pnode->location);
            byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, doneLabel, pnode->sxUni.pnode1->location);
            byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, pnode->location);
            byteCodeGenerator->Writer()->MarkLabel(doneLabel);
        }
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        break;
    }
    // PTNODE(knopEllipsis     , "..."       ,Spread  ,Uni         , fnopUni)
    case knopEllipsis:
    {
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        // Transparently pass the location of the array.
        pnode->location = pnode->sxUni.pnode1->location;
        break;
    }
    // PTNODE(knopIncPost    , "post++"    ,Inc     ,Uni  ,fnopUni|fnopAsg)
    case knopIncPost:
    case knopDecPost:
        // FALL THROUGH to the faster pre-inc/dec case if the result of the expression is not needed.
        if (pnode->isUsed || fReturnValue)
        {
            byteCodeGenerator->StartStatement(pnode);
            Js::OpCode op = Js::OpCode::Add_A;
            if (pnode->nop == knopDecPost)
            {
                op = Js::OpCode::Sub_A;
            }
            // Grab a register for the expression result.
            funcInfo->AcquireLoc(pnode);

            // Load the initial value, convert it (this is the expression result), and increment it.
            EmitLoad(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Conv_Num, pnode->location, pnode->sxUni.pnode1->location);

            Js::RegSlot incDecResult = pnode->sxUni.pnode1->location;
            if (funcInfo->RegIsConst(incDecResult))
            {
                // Avoid letting the add/sub overwrite a constant reg, as this may actually change the
                // contents of the constant table.
                incDecResult = funcInfo->AcquireTmpRegister();
            }

            Js::RegSlot oneReg = funcInfo->constantToRegister.LookupWithKey(1, Js::Constants::NoRegister);
            Assert(oneReg != Js::Constants::NoRegister);
            byteCodeGenerator->Writer()->Reg3(op, incDecResult, pnode->location, oneReg);

            // Store the incremented value.
            EmitAssignment(nullptr, pnode->sxUni.pnode1, incDecResult, byteCodeGenerator, funcInfo);

            // Release the incremented value and the l-value.
            if (incDecResult != pnode->sxUni.pnode1->location)
            {
                funcInfo->ReleaseTmpRegister(incDecResult);
            }
            funcInfo->ReleaseLoad(pnode->sxUni.pnode1);
            byteCodeGenerator->EndStatement(pnode);

            break;
        }
        else
        {
            pnode->nop = (pnode->nop == knopIncPost) ? knopIncPre : knopDecPre;
        }
        // FALL THROUGH to the fast pre-inc/dec case if the result of the expression is not needed.

    // PTNODE(knopIncPre     , "++ pre"    ,Inc     ,Uni  ,fnopUni|fnopAsg)
    case knopIncPre:
    case knopDecPre:
    {
        byteCodeGenerator->StartStatement(pnode);
        Js::OpCode op = Js::OpCode::Incr_A;
        if (pnode->nop == knopDecPre)
        {
            op = Js::OpCode::Decr_A;
        }

        // Assign a register for the result only if the result is used or the operand can't be assigned to
        // (i.e., is a constant).
        if (pnode->isUsed || fReturnValue)
        {
            funcInfo->AcquireLoc(pnode);

            // Load the initial value and increment it (this is the expression result).
            EmitLoad(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo);
            byteCodeGenerator->Writer()->Reg2(op, pnode->location, pnode->sxUni.pnode1->location);

            // Store the incremented value and release the l-value.
            EmitAssignment(nullptr, pnode->sxUni.pnode1, pnode->location, byteCodeGenerator, funcInfo);
            funcInfo->ReleaseLoad(pnode->sxUni.pnode1);
        }
        else
        {
            // Load the initial value and increment it (this is the expression result).
            EmitLoad(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo);

            Js::RegSlot incDecResult = pnode->sxUni.pnode1->location;
            if (funcInfo->RegIsConst(incDecResult))
            {
                // Avoid letting the add/sub overwrite a constant reg, as this may actually change the
                // contents of the constant table.
                incDecResult = funcInfo->AcquireTmpRegister();
            }

            byteCodeGenerator->Writer()->Reg2(op, incDecResult, pnode->sxUni.pnode1->location);

            // Store the incremented value and release the l-value.
            EmitAssignment(nullptr, pnode->sxUni.pnode1, incDecResult, byteCodeGenerator, funcInfo);
            if (incDecResult != pnode->sxUni.pnode1->location)
            {
                funcInfo->ReleaseTmpRegister(incDecResult);
            }
            funcInfo->ReleaseLoad(pnode->sxUni.pnode1);
        }

        byteCodeGenerator->EndStatement(pnode);
        break;
    }
    // PTNODE(knopTypeof     , "typeof"    ,None    ,Uni  ,fnopUni)
    case knopTypeof:
    {
        ParseNode* pnodeOpnd = pnode->sxUni.pnode1;
        switch (pnodeOpnd->nop)
        {
        case knopDot:
        {
            Emit(pnodeOpnd->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
            Js::PropertyId propertyId = pnodeOpnd->sxBin.pnode2->sxPid.PropertyIdFromNameNode();
            Assert(pnodeOpnd->sxBin.pnode2->nop == knopName);
            funcInfo->ReleaseLoc(pnodeOpnd->sxBin.pnode1);
            funcInfo->AcquireLoc(pnode);

            byteCodeGenerator->EmitTypeOfFld(funcInfo, propertyId, pnode->location, pnodeOpnd->sxBin.pnode1->location, Js::OpCode::LdFldForTypeOf);
            break;
        }

        case knopIndex:
        {
            EmitBinaryOpnds(pnodeOpnd->sxBin.pnode1, pnodeOpnd->sxBin.pnode2, byteCodeGenerator, funcInfo);
            funcInfo->ReleaseLoc(pnodeOpnd->sxBin.pnode2);
            funcInfo->ReleaseLoc(pnodeOpnd->sxBin.pnode1);
            funcInfo->AcquireLoc(pnode);
            byteCodeGenerator->Writer()->Element(Js::OpCode::TypeofElem, pnode->location, pnodeOpnd->sxBin.pnode1->location, pnodeOpnd->sxBin.pnode2->location);
            break;
        }
        case knopName:
        {
            funcInfo->AcquireLoc(pnode);
            byteCodeGenerator->EmitPropTypeof(pnode->location, pnodeOpnd->sxPid.sym, pnodeOpnd->sxPid.pid, funcInfo);
            break;
        }

        default:
            Emit(pnodeOpnd, byteCodeGenerator, funcInfo, false);
            funcInfo->ReleaseLoc(pnodeOpnd);
            byteCodeGenerator->Writer()->Reg2(
                Js::OpCode::Typeof, funcInfo->AcquireLoc(pnode), pnodeOpnd->location);
            break;
        }
        break;
    }
    // PTNODE(knopVoid       , "void"        ,Void    ,Uni  ,fnopUni)
    case knopVoid:
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdUndef, funcInfo->AcquireLoc(pnode));
        break;
    // PTNODE(knopArray      , "arr cnst"    ,None    ,Uni  ,fnopUni)
    case knopArray:
        EmitArrayLiteral(pnode, byteCodeGenerator, funcInfo);
        break;
    // PTNODE(knopObject     , "obj cnst"    ,None    ,Uni  ,fnopUni)
    case knopObject:
        funcInfo->AcquireLoc(pnode);
        EmitObjectInitializers(pnode->sxUni.pnode1, pnode->location, byteCodeGenerator, funcInfo);
        break;
    // PTNODE(knopComputedName, "[name]"      ,None    ,Uni  ,fnopUni)
    case knopComputedName:
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        if (pnode->location == Js::Constants::NoRegister)
        {
            // The name is some expression with no home location. We can just re-use the register.
            pnode->location = pnode->sxUni.pnode1->location;
        }
        else if (pnode->location != pnode->sxUni.pnode1->location)
        {
            // The name had to be protected from side-effects of the RHS.
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxUni.pnode1->location);
        }
        break;
    // Binary and Ternary Operators
    case knopAdd:
        EmitAdd(pnode, byteCodeGenerator, funcInfo);
        break;
    case knopSub:
    case knopMul:
    case knopExpo:
    case knopDiv:
    case knopMod:
    case knopOr:
    case knopXor:
    case knopAnd:
    case knopLsh:
    case knopRsh:
    case knopRs2:
    case knopIn:
        EmitBinary(nopToOp[pnode->nop], pnode, byteCodeGenerator, funcInfo);
        break;
    case knopInstOf:
    {
        byteCodeGenerator->StartStatement(pnode);
        EmitBinaryOpnds(pnode->sxBin.pnode1, pnode->sxBin.pnode2, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode1);
        funcInfo->AcquireLoc(pnode);
        uint cacheId = funcInfo->NewIsInstInlineCache();
        byteCodeGenerator->Writer()->Reg3C(nopToOp[pnode->nop], pnode->location, pnode->sxBin.pnode1->location,
            pnode->sxBin.pnode2->location, cacheId);
        byteCodeGenerator->EndStatement(pnode);
    }
    break;
    case knopEq:
    case knopEqv:
    case knopNEqv:
    case knopNe:
    case knopLt:
    case knopLe:
    case knopGe:
    case knopGt:
        byteCodeGenerator->StartStatement(pnode);
        EmitBinaryOpnds(pnode->sxBin.pnode1, pnode->sxBin.pnode2, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode1);
        funcInfo->AcquireLoc(pnode);
        byteCodeGenerator->Writer()->Reg3(nopToCMOp[pnode->nop], pnode->location, pnode->sxBin.pnode1->location,
            pnode->sxBin.pnode2->location);
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopNew:
    {
        Js::ArgSlot argCount = pnode->sxCall.argCount;
        argCount++; // include "this"

        BOOL fSideEffectArgs = FALSE;
        unsigned int tmpCount = CountArguments(pnode->sxCall.pnodeArgs, &fSideEffectArgs);
        Assert(argCount == tmpCount);

        if (argCount != (Js::ArgSlot)argCount)
        {
            Js::Throw::OutOfMemory();
        }

        byteCodeGenerator->StartStatement(pnode);

        // Start call, allocate out param space
        funcInfo->StartRecordingOutArgs(argCount);

        // Assign the call target operand(s), putting them into expression temps if necessary to protect
        // them from side-effects.
        if (fSideEffectArgs)
        {
            SaveOpndValue(pnode->sxCall.pnodeTarget, funcInfo);
        }

        if (pnode->sxCall.pnodeTarget->nop == knopSuper)
        {
            EmitSuperFieldPatch(funcInfo, pnode, byteCodeGenerator);
        }

        Emit(pnode->sxCall.pnodeTarget, byteCodeGenerator, funcInfo, false, true);

        if (pnode->sxCall.pnodeArgs == nullptr)
        {
            funcInfo->ReleaseLoc(pnode->sxCall.pnodeTarget);
            Js::OpCode op = (CreateNativeArrays(byteCodeGenerator, funcInfo)
                && CallTargetIsArray(pnode->sxCall.pnodeTarget))
                ? Js::OpCode::NewScObjArray : Js::OpCode::NewScObject;
            Assert(argCount == 1);

            Js::ProfileId callSiteId = byteCodeGenerator->GetNextCallSiteId(op);
            byteCodeGenerator->Writer()->StartCall(Js::OpCode::StartCall, argCount);
            byteCodeGenerator->Writer()->CallI(op, funcInfo->AcquireLoc(pnode),
                pnode->sxCall.pnodeTarget->location, argCount, callSiteId);
        }
        else
        {
            byteCodeGenerator->Writer()->StartCall(Js::OpCode::StartCall, argCount);
            uint32 actualArgCount = 0;

            if (IsCallOfConstants(pnode))
            {
                funcInfo->ReleaseLoc(pnode->sxCall.pnodeTarget);
                actualArgCount = EmitNewObjectOfConstants(pnode, byteCodeGenerator, funcInfo, argCount);
            }
            else
            {
                Js::OpCode op;
                if ((CreateNativeArrays(byteCodeGenerator, funcInfo) && CallTargetIsArray(pnode->sxCall.pnodeTarget)))
                {
                    op = pnode->sxCall.spreadArgCount > 0 ? Js::OpCode::NewScObjArraySpread : Js::OpCode::NewScObjArray;
                }
                else
                {
                    op = pnode->sxCall.spreadArgCount > 0 ? Js::OpCode::NewScObjectSpread : Js::OpCode::NewScObject;
                }

                Js::ProfileId callSiteId = byteCodeGenerator->GetNextCallSiteId(op);


                Js::AuxArray<uint32> *spreadIndices = nullptr;
                actualArgCount = EmitArgList(pnode->sxCall.pnodeArgs, Js::Constants::NoRegister, Js::Constants::NoRegister, Js::Constants::NoRegister,
                    false, true, byteCodeGenerator, funcInfo, callSiteId, pnode->sxCall.spreadArgCount, &spreadIndices);
                funcInfo->ReleaseLoc(pnode->sxCall.pnodeTarget);


                if (pnode->sxCall.spreadArgCount > 0)
                {
                    Assert(spreadIndices != nullptr);
                    uint spreadExtraAlloc = spreadIndices->count * sizeof(uint32);
                    uint spreadIndicesSize = sizeof(*spreadIndices) + spreadExtraAlloc;
                    byteCodeGenerator->Writer()->CallIExtended(op, funcInfo->AcquireLoc(pnode), pnode->sxCall.pnodeTarget->location,
                        (uint16)actualArgCount, Js::CallIExtended_SpreadArgs,
                        spreadIndices, spreadIndicesSize, callSiteId);
                }
                else
                {
                    byteCodeGenerator->Writer()->CallI(op, funcInfo->AcquireLoc(pnode), pnode->sxCall.pnodeTarget->location,
                        (uint16)actualArgCount, callSiteId);
                }
            }

            Assert(argCount == actualArgCount);
        }

        // End call, pop param space
        funcInfo->EndRecordingOutArgs(argCount);

        byteCodeGenerator->EndStatement(pnode);
        break;
    }
    case knopDelete:
    {
        ParseNode *pexpr = pnode->sxUni.pnode1;
        byteCodeGenerator->StartStatement(pnode);
        switch (pexpr->nop)
        {
        case knopName:
        {
            funcInfo->AcquireLoc(pnode);
            byteCodeGenerator->EmitPropDelete(pnode->location, pexpr->sxPid.sym, pexpr->sxPid.pid, funcInfo);
            break;
        }
        case knopDot:
        {
            Emit(pexpr->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
            Js::PropertyId propertyId = pexpr->sxBin.pnode2->sxPid.PropertyIdFromNameNode();
            funcInfo->ReleaseLoc(pexpr->sxBin.pnode1);
            funcInfo->AcquireLoc(pnode);
            byteCodeGenerator->Writer()->Property(Js::OpCode::DeleteFld, pnode->location, pexpr->sxBin.pnode1->location,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));
            break;
        }
        case knopIndex:
        {
            EmitBinaryOpnds(pexpr->sxBin.pnode1, pexpr->sxBin.pnode2, byteCodeGenerator, funcInfo);
            funcInfo->ReleaseLoc(pexpr->sxBin.pnode2);
            funcInfo->ReleaseLoc(pexpr->sxBin.pnode1);
            funcInfo->AcquireLoc(pnode);
            byteCodeGenerator->Writer()->Element(Js::OpCode::DeleteElemI_A, pnode->location, pexpr->sxBin.pnode1->location, pexpr->sxBin.pnode2->location);
            break;
        }
        case knopThis:
        {
            funcInfo->AcquireLoc(pnode);
            byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, pnode->location);
            break;
        }
        default:
        {
            Emit(pexpr, byteCodeGenerator, funcInfo, false);
            funcInfo->ReleaseLoc(pexpr);
            byteCodeGenerator->Writer()->Reg2(
                Js::OpCode::Delete_A, funcInfo->AcquireLoc(pnode), pexpr->location);
            break;
        }
        }
        byteCodeGenerator->EndStatement(pnode);
        break;
    }
    case knopCall:
    {
        byteCodeGenerator->StartStatement(pnode);

        if (pnode->sxCall.pnodeTarget->nop == knopSuper)
        {
            byteCodeGenerator->EmitSuperCall(funcInfo, pnode, fReturnValue);
        }
        else
        {
            if (pnode->sxCall.isApplyCall && funcInfo->GetApplyEnclosesArgs())
            {
                // TODO[ianhall]: Can we remove the ApplyCall bytecode gen time optimization?
                EmitApplyCall(pnode, Js::Constants::NoRegister, byteCodeGenerator, funcInfo, fReturnValue, true);
            }
            else
            {
                EmitCall(pnode, Js::Constants::NoRegister, byteCodeGenerator, funcInfo, fReturnValue, /*fEvaluateComponents*/ true, /*fHasNewTarget*/ false);
            }
        }

        byteCodeGenerator->EndStatement(pnode);
        break;
    }
    case knopIndex:
    {
        byteCodeGenerator->StartStatement(pnode);
        EmitBinaryOpnds(pnode->sxBin.pnode1, pnode->sxBin.pnode2, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode1);
        funcInfo->AcquireLoc(pnode);

        Js::RegSlot callObjLocation = pnode->sxBin.pnode1->location;
        Js::RegSlot protoLocation = callObjLocation;
        EmitSuperMethodBegin(pnode, byteCodeGenerator, funcInfo);
        byteCodeGenerator->Writer()->Element(
            Js::OpCode::LdElemI_A, pnode->location, protoLocation, pnode->sxBin.pnode2->location);
        byteCodeGenerator->EndStatement(pnode);
        break;
    }
    // this is MemberExpression as rvalue
    case knopDot:
    {
        Emit(pnode->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode1);
        funcInfo->AcquireLoc(pnode);
        Js::PropertyId propertyId = pnode->sxBin.pnode2->sxPid.PropertyIdFromNameNode();

        Js::RegSlot callObjLocation = pnode->sxBin.pnode1->location;
        Js::RegSlot protoLocation = callObjLocation;
        EmitSuperMethodBegin(pnode, byteCodeGenerator, funcInfo);

        if (propertyId == Js::PropertyIds::length)
        {
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::LdLen_A, pnode->location, protoLocation);
        }
        else
        {
            uint cacheId = funcInfo->FindOrAddInlineCacheId(callObjLocation, propertyId, false, false);
            if (pnode->IsCallApplyTargetLoad())
            {
                byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFldForCallApplyTarget, pnode->location, protoLocation, cacheId);
            }
            else
            {
                if (pnode->sxBin.pnode1->nop == knopSuper)
                {
                    byteCodeGenerator->Writer()->PatchablePropertyWithThisPtr(Js::OpCode::LdSuperFld, pnode->location, protoLocation, funcInfo->thisPointerRegister, cacheId, isConstructorCall);
                }
                else
                {
                    byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, pnode->location, callObjLocation, cacheId, isConstructorCall);
                }
            }
        }

        break;
    }

    // PTNODE(knopAsg        , "="            ,None    ,Bin  ,fnopBin|fnopAsg)
    case knopAsg:
    {
        ParseNode *lhs = pnode->sxBin.pnode1;
        ParseNode *rhs = pnode->sxBin.pnode2;
        byteCodeGenerator->StartStatement(pnode);
        if (pnode->isUsed || fReturnValue)
        {
            // If the assignment result is used, grab a register to hold it and pass it to EmitAssignment,
            // which will copy the assigned value there.
            funcInfo->AcquireLoc(pnode);
            EmitBinaryReference(lhs, rhs, byteCodeGenerator, funcInfo, false);
            EmitAssignment(pnode, lhs, rhs->location, byteCodeGenerator, funcInfo);
        }
        else
        {
            EmitBinaryReference(lhs, rhs, byteCodeGenerator, funcInfo, false);
            EmitAssignment(nullptr, lhs, rhs->location, byteCodeGenerator, funcInfo);
        }
        funcInfo->ReleaseLoc(rhs);
        if (!(byteCodeGenerator->IsES6DestructuringEnabled() && (lhs->IsPattern())))
        {
            funcInfo->ReleaseReference(lhs);
        }
        byteCodeGenerator->EndStatement(pnode);
        break;
    }

    case knopName:
        funcInfo->AcquireLoc(pnode);
        byteCodeGenerator->EmitPropLoad(pnode->location, pnode->sxPid.sym, pnode->sxPid.pid, funcInfo);
        break;

    case knopComma:
    {
        byteCodeGenerator->StartStatement(pnode);
        // The parser marks binary opnd pnodes as used, but value of the first opnd of a comma is not used.
        // Easier to correct this here than to check every binary op in the parser.
        ParseNode *pnode1 = pnode->sxBin.pnode1;
        pnode1->isUsed = false;
        if (pnode1->nop == knopComma)
        {
            // Spot fix for giant comma expressions that send us into OOS if we use a simple recursive
            // algorithm. Instead of recursing on comma LHS's, iterate over them, pushing the RHS's onto
            // a stack. (This suggests a model for removing recursion from Emit altogether...)
            ArenaAllocator *alloc = byteCodeGenerator->GetAllocator();
            SList<ParseNode *> rhsStack(alloc);
            do
            {
                rhsStack.Push(pnode1->sxBin.pnode2);
                pnode1 = pnode1->sxBin.pnode1;
                pnode1->isUsed = false;
            }
            while (pnode1->nop == knopComma);

            Emit(pnode1, byteCodeGenerator, funcInfo, false);
            if (funcInfo->IsTmpReg(pnode1->location))
            {
                byteCodeGenerator->Writer()->Reg1(Js::OpCode::Unused, pnode1->location);
            }

            while (!rhsStack.Empty())
            {
                ParseNode *pnodeRhs = rhsStack.Pop();
                pnodeRhs->isUsed = false;
                Emit(pnodeRhs, byteCodeGenerator, funcInfo, false);
                if (funcInfo->IsTmpReg(pnodeRhs->location))
                {
                    byteCodeGenerator->Writer()->Reg1(Js::OpCode::Unused, pnodeRhs->location);
                }
                funcInfo->ReleaseLoc(pnodeRhs);
            }
        }
        else
        {
            Emit(pnode1, byteCodeGenerator, funcInfo, false);
            if (funcInfo->IsTmpReg(pnode1->location))
            {
                byteCodeGenerator->Writer()->Reg1(Js::OpCode::Unused, pnode1->location);
            }
        }
        funcInfo->ReleaseLoc(pnode1);

        pnode->sxBin.pnode2->isUsed = pnode->isUsed || fReturnValue;
        Emit(pnode->sxBin.pnode2, byteCodeGenerator, funcInfo, false);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
        funcInfo->AcquireLoc(pnode);
        if (pnode->sxBin.pnode2->isUsed && pnode->location != pnode->sxBin.pnode2->location)
        {
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxBin.pnode2->location);
        }
        byteCodeGenerator->EndStatement(pnode);
    }
    break;

    // The binary logical ops && and || resolve to the value of the left-hand expression if its
    // boolean value short-circuits the operation, and to the value of the right-hand expression
    // otherwise. (In other words, the "truth" of the right-hand expression is never tested.)
    // PTNODE(knopLogOr      , "||"        ,None    ,Bin  ,fnopBin)
    case knopLogOr:
    {
        Js::ByteCodeLabel doneLabel = byteCodeGenerator->Writer()->DefineLabel();
        // For boolean expressions that compute a result, we have to burn a register for the result
        // so that the back end can identify it cheaply as a single temp lifetime. Revisit this if we do
        // full-on renaming in the back end.
        funcInfo->AcquireLoc(pnode);

        Emit(pnode->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxBin.pnode1->location);
        byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, doneLabel, pnode->sxBin.pnode1->location);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode1);

        Emit(pnode->sxBin.pnode2, byteCodeGenerator, funcInfo, false);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxBin.pnode2->location);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
        byteCodeGenerator->Writer()->MarkLabel(doneLabel);
        break;
    }
    // PTNODE(knopLogAnd     , "&&"        ,None    ,Bin  ,fnopBin)
    case knopLogAnd:
    {
        Js::ByteCodeLabel doneLabel = byteCodeGenerator->Writer()->DefineLabel();
        // For boolean expressions that compute a result, we have to burn a register for the result
        // so that the back end can identify it cheaply as a single temp lifetime. Revisit this if we do
        // full-on renaming in the back end.
        funcInfo->AcquireLoc(pnode);

        Emit(pnode->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxBin.pnode1->location);
        byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrFalse_A, doneLabel, pnode->sxBin.pnode1->location);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode1);

        Emit(pnode->sxBin.pnode2, byteCodeGenerator, funcInfo, false);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxBin.pnode2->location);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
        byteCodeGenerator->Writer()->MarkLabel(doneLabel);
        break;
    }
    // PTNODE(knopQmark      , "?"            ,None    ,Tri  ,fnopBin)
    case knopQmark:
    {
        Js::ByteCodeLabel trueLabel = byteCodeGenerator->Writer()->DefineLabel();
        Js::ByteCodeLabel falseLabel = byteCodeGenerator->Writer()->DefineLabel();
        Js::ByteCodeLabel skipLabel = byteCodeGenerator->Writer()->DefineLabel();
        EmitBooleanExpression(pnode->sxTri.pnode1, trueLabel, falseLabel, byteCodeGenerator, funcInfo);
        byteCodeGenerator->Writer()->MarkLabel(trueLabel);
        funcInfo->ReleaseLoc(pnode->sxTri.pnode1);

        // For boolean expressions that compute a result, we have to burn a register for the result
        // so that the back end can identify it cheaply as a single temp lifetime. Revisit this if we do
        // full-on renaming in the back end.
        funcInfo->AcquireLoc(pnode);

        Emit(pnode->sxTri.pnode2, byteCodeGenerator, funcInfo, false);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxTri.pnode2->location);
        funcInfo->ReleaseLoc(pnode->sxTri.pnode2);

        // Record the branch bytecode offset
        byteCodeGenerator->Writer()->RecordStatementAdjustment(Js::FunctionBody::SAT_FromCurrentToNext);

        byteCodeGenerator->Writer()->Br(skipLabel);

        byteCodeGenerator->Writer()->MarkLabel(falseLabel);
        Emit(pnode->sxTri.pnode3, byteCodeGenerator, funcInfo, false);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxTri.pnode3->location);
        funcInfo->ReleaseLoc(pnode->sxTri.pnode3);

        byteCodeGenerator->Writer()->MarkLabel(skipLabel);

        break;
    }

    case knopAsgAdd:
    case knopAsgSub:
    case knopAsgMul:
    case knopAsgDiv:
    case knopAsgExpo:
    case knopAsgMod:
    case knopAsgAnd:
    case knopAsgXor:
    case knopAsgOr:
    case knopAsgLsh:
    case knopAsgRsh:
    case knopAsgRs2:
        byteCodeGenerator->StartStatement(pnode);
        // Assign a register for the result only if the result is used or the LHS can't be assigned to
        // (i.e., is a constant).
        if (pnode->isUsed || fReturnValue || funcInfo->RegIsConst(pnode->sxBin.pnode1->location))
        {
            // If the assign-op result is used, grab a register to hold it.
            funcInfo->AcquireLoc(pnode);

            // Grab a register for the initial value and load it.
            EmitBinaryReference(pnode->sxBin.pnode1, pnode->sxBin.pnode2, byteCodeGenerator, funcInfo, true);
            funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
            // Do the arithmetic, store the result, and release the l-value.
            byteCodeGenerator->Writer()->Reg3(nopToOp[pnode->nop], pnode->location, pnode->sxBin.pnode1->location,
                pnode->sxBin.pnode2->location);

            EmitAssignment(pnode, pnode->sxBin.pnode1, pnode->location, byteCodeGenerator, funcInfo);
        }
        else
        {
            // Grab a register for the initial value and load it.
            EmitBinaryReference(pnode->sxBin.pnode1, pnode->sxBin.pnode2, byteCodeGenerator, funcInfo, true);
            funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
            // Do the arithmetic, store the result, and release the l-value.
            byteCodeGenerator->Writer()->Reg3(nopToOp[pnode->nop], pnode->sxBin.pnode1->location, pnode->sxBin.pnode1->location,
                pnode->sxBin.pnode2->location);
            EmitAssignment(nullptr, pnode->sxBin.pnode1, pnode->sxBin.pnode1->location, byteCodeGenerator, funcInfo);
        }
        funcInfo->ReleaseLoad(pnode->sxBin.pnode1);

        byteCodeGenerator->EndStatement(pnode);
        break;

    // General nodes.
    // PTNODE(knopTempRef      , "temp ref"  ,None   ,Uni ,fnopUni)
    case knopTempRef:
        // TODO: check whether mov is necessary
        funcInfo->AcquireLoc(pnode);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxUni.pnode1->location);
        break;
    // PTNODE(knopTemp      , "temp"        ,None   ,None ,fnopLeaf)
    case knopTemp:
        // Emit initialization code
        if (pnode->sxVar.pnodeInit != nullptr)
        {
            byteCodeGenerator->StartStatement(pnode);
            Emit(pnode->sxVar.pnodeInit, byteCodeGenerator, funcInfo, false);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxVar.pnodeInit->location);
            funcInfo->ReleaseLoc(pnode->sxVar.pnodeInit);
            byteCodeGenerator->EndStatement(pnode);
        }
        break;
    // PTNODE(knopVarDecl    , "varDcl"    ,None    ,Var  ,fnopNone)
    case knopVarDecl:
    {
        // Emit initialization code
        ParseNodePtr initNode = pnode->sxVar.pnodeInit;

        if (initNode != nullptr)
        {
            byteCodeGenerator->StartStatement(pnode);
            Emit(pnode->sxVar.pnodeInit, byteCodeGenerator, funcInfo, false);
            EmitAssignment(nullptr, pnode, pnode->sxVar.pnodeInit->location, byteCodeGenerator, funcInfo);
            funcInfo->ReleaseLoc(pnode->sxVar.pnodeInit);
            byteCodeGenerator->EndStatement(pnode);

            Symbol *sym = pnode->sxVar.sym;
            if (pnode->sxVar.pnodeInit->nop == knopObject && (ParseNode::Grfnop(pnode->sxVar.pnodeInit->nop) & fnopUni))
            {
                TrackMemberNodesInObjectForIntConstants(byteCodeGenerator, pnode->sxVar.pnodeInit);
            }
            else if (initNode->nop == knopInt)
            {
                TrackIntConstantsOnGlobalObject(byteCodeGenerator, sym);
            }
        }
        break;
    }
    case knopConstDecl:
    {
        ParseNodePtr initNode = pnode->sxVar.pnodeInit;
        Symbol * sym = pnode->sxVar.sym;

        byteCodeGenerator->StartStatement(pnode);

        if (initNode)
        {
            Emit(initNode, byteCodeGenerator, funcInfo, false);

            byteCodeGenerator->EmitPropStore(initNode->location, sym, nullptr, funcInfo, false, true);
            funcInfo->ReleaseLoc(pnode->sxVar.pnodeInit);

            if (initNode->nop == knopInt)
            {
                TrackIntConstantsOnGlobalObject(byteCodeGenerator, sym);
            }
        }

        byteCodeGenerator->EndStatement(pnode);

        // Set NeedDeclaration to false AFTER emitting the initializer, otherwise we won't have "Use Before Declaration"
        // errors reported in the initializer for code like this:
        // { const a = a; }
        sym->SetNeedDeclaration(false);

        break;
    }
    case knopLetDecl:
    {
        ParseNodePtr initNode = pnode->sxVar.pnodeInit;
        byteCodeGenerator->StartStatement(pnode);

        Symbol * sym = pnode->sxVar.sym;
        Js::RegSlot rhsLocation;
        if (pnode->sxVar.pnodeInit != nullptr)
        {
            Emit(pnode->sxVar.pnodeInit, byteCodeGenerator, funcInfo, false);
            rhsLocation = pnode->sxVar.pnodeInit->location;
            if (initNode->nop == knopInt)
            {
                TrackIntConstantsOnGlobalObject(byteCodeGenerator, sym);
            }
        }
        else
        {
            rhsLocation = funcInfo->AcquireTmpRegister();
            byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdUndef, rhsLocation);
        }

        // Set NeedDeclaration to false AFTER emitting the initializer, otherwise we won't have "Use Before Declaration"
        // errors reported in the initializer for code like this:
        // { let a = a; }
        sym->SetNeedDeclaration(false);

        byteCodeGenerator->EmitPropStore(rhsLocation, sym, nullptr, funcInfo, true, false);

        funcInfo->ReleaseTmpRegister(rhsLocation);
        byteCodeGenerator->EndStatement(pnode);

        break;
    }
    // PTNODE(knopFncDecl    , "fncDcl"    ,None    ,Fnc  ,fnopLeaf)
    case knopFncDecl:
        // The "function declarations" were emitted in DefineFunctions()
        if (!pnode->sxFnc.IsDeclaration())
        {
            byteCodeGenerator->DefineOneFunction(pnode, funcInfo, false);
        }
        break;
    // PTNODE(knopClassDecl, "class"    ,None    ,None ,fnopLeaf)
    case knopClassDecl:
    {
        funcInfo->AcquireLoc(pnode);

        // Extends
        if (pnode->sxClass.pnodeExtends)
        {
            Emit(pnode->sxClass.pnodeExtends, byteCodeGenerator, funcInfo, false);
        }

        Assert(pnode->sxClass.pnodeConstructor);
        pnode->sxClass.pnodeConstructor->location = pnode->location;

        BeginEmitBlock(pnode->sxClass.pnodeBlock, byteCodeGenerator, funcInfo);

        // Constructor
        Emit(pnode->sxClass.pnodeConstructor, byteCodeGenerator, funcInfo, false);
        EmitComputedFunctionNameVar(bindPnode, pnode->sxClass.pnodeConstructor, byteCodeGenerator);
        if (pnode->sxClass.pnodeExtends)
        {
            byteCodeGenerator->Writer()->InitClass(pnode->location, pnode->sxClass.pnodeExtends->location);
        }
        else
        {
            byteCodeGenerator->Writer()->InitClass(pnode->location);
        }

            Js::RegSlot protoLoc = funcInfo->AcquireTmpRegister(); //register set if we have Instance Methods
            int cacheId = funcInfo->FindOrAddInlineCacheId(pnode->location, Js::PropertyIds::prototype, false, false);
            byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, protoLoc, pnode->location, cacheId);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::SetHomeObj, pnode->location, protoLoc);

        // Static Methods
        EmitClassInitializers(pnode->sxClass.pnodeStaticMembers, pnode->location, byteCodeGenerator, funcInfo, pnode, /*isObjectEmpty*/ false);

        // Instance Methods
        EmitClassInitializers(pnode->sxClass.pnodeMembers, protoLoc, byteCodeGenerator, funcInfo, pnode, /*isObjectEmpty*/ true);
        funcInfo->ReleaseTmpRegister(protoLoc);

        // Emit name binding.
        if (pnode->sxClass.pnodeName)
        {
            Symbol * sym = pnode->sxClass.pnodeName->sxVar.sym;
            sym->SetNeedDeclaration(false);
            byteCodeGenerator->EmitPropStore(pnode->location, sym, nullptr, funcInfo, false, true);
        }

        EndEmitBlock(pnode->sxClass.pnodeBlock, byteCodeGenerator, funcInfo);

        if (pnode->sxClass.pnodeExtends)
        {
            funcInfo->ReleaseLoc(pnode->sxClass.pnodeExtends);
        }

        if (pnode->sxClass.pnodeDeclName)
        {
            Symbol * sym = pnode->sxClass.pnodeDeclName->sxVar.sym;
            sym->SetNeedDeclaration(false);
            byteCodeGenerator->EmitPropStore(pnode->location, sym, nullptr, funcInfo, true, false);
        }

        break;
    }
    case knopStrTemplate:
        EmitStringTemplate(pnode, byteCodeGenerator, funcInfo);
        break;
    case knopEndCode:
        byteCodeGenerator->Writer()->RecordStatementAdjustment(Js::FunctionBody::SAT_All);

        // load undefined for the fallthrough case:
        if (!funcInfo->IsGlobalFunction())
        {
            if (funcInfo->IsClassConstructor())
            {
                // For class constructors, we need to explicitly load 'this' into the return register.
                byteCodeGenerator->EmitClassConstructorEndCode(funcInfo);
            }
            else
            {
                // In the global function, implicit return values are copied to the return register, and if
                // necessary the return register is initialized at the top. Don't clobber the value here.
                byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdUndef, ByteCodeGenerator::ReturnRegister);
            }
        }

        // Label for non-fall-through return
        byteCodeGenerator->Writer()->MarkLabel(funcInfo->singleExit);

        if (funcInfo->GetHasCachedScope())
        {
            byteCodeGenerator->Writer()->AuxNoReg(
                Js::OpCode::CommitScope,
                funcInfo->localPropIdOffset,
                sizeof(Js::PropertyIdArray) + ((funcInfo->GetBodyScope()->GetScopeSlotCount() + 3) * sizeof(Js::PropertyId)));
        }
        byteCodeGenerator->StartStatement(pnode);
        byteCodeGenerator->Writer()->Empty(Js::OpCode::Ret);
        byteCodeGenerator->EndStatement(pnode);
        break;
    // PTNODE(knopDebugger   , "debugger"    ,None    ,None ,fnopNone)
    case knopDebugger:
        byteCodeGenerator->StartStatement(pnode);
        byteCodeGenerator->Writer()->Empty(Js::OpCode::Break);
        byteCodeGenerator->EndStatement(pnode);
        break;
    // PTNODE(knopFor        , "for"        ,None    ,For  ,fnopBreak|fnopContinue)
    case knopFor:
        if (pnode->sxFor.pnodeInverted != nullptr)
        {
            byteCodeGenerator->EmitInvertedLoop(pnode, pnode->sxFor.pnodeInverted, funcInfo);
        }
        else
        {
            BeginEmitBlock(pnode->sxFor.pnodeBlock, byteCodeGenerator, funcInfo);
            Emit(pnode->sxFor.pnodeInit, byteCodeGenerator, funcInfo, false);
            funcInfo->ReleaseLoc(pnode->sxFor.pnodeInit);
            if (byteCodeGenerator->IsES6ForLoopSemanticsEnabled())
            {
                CloneEmitBlock(pnode->sxFor.pnodeBlock, byteCodeGenerator, funcInfo);
            }
            EmitLoop(pnode,
                pnode->sxFor.pnodeCond,
                pnode->sxFor.pnodeBody,
                pnode->sxFor.pnodeIncr,
                byteCodeGenerator,
                funcInfo,
                fReturnValue,
                FALSE,
                pnode->sxFor.pnodeBlock);
            EndEmitBlock(pnode->sxFor.pnodeBlock, byteCodeGenerator, funcInfo);
        }
        break;
    // PTNODE(knopIf         , "if"        ,None    ,If   ,fnopNone)
    case knopIf:
    {
        byteCodeGenerator->StartStatement(pnode);

        Js::ByteCodeLabel trueLabel = byteCodeGenerator->Writer()->DefineLabel();
        Js::ByteCodeLabel falseLabel = byteCodeGenerator->Writer()->DefineLabel();
        EmitBooleanExpression(pnode->sxIf.pnodeCond, trueLabel, falseLabel, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(pnode->sxIf.pnodeCond);

        byteCodeGenerator->EndStatement(pnode);

        byteCodeGenerator->Writer()->MarkLabel(trueLabel);
        Emit(pnode->sxIf.pnodeTrue, byteCodeGenerator, funcInfo, fReturnValue);
        funcInfo->ReleaseLoc(pnode->sxIf.pnodeTrue);
        if (pnode->sxIf.pnodeFalse != nullptr)
        {
            // has else clause
            Js::ByteCodeLabel skipLabel = byteCodeGenerator->Writer()->DefineLabel();

            // Record the branch bytecode offset
            byteCodeGenerator->Writer()->RecordStatementAdjustment(Js::FunctionBody::SAT_FromCurrentToNext);

            // then clause skips else clause
            byteCodeGenerator->Writer()->Br(skipLabel);
            // generate code for else clause
            byteCodeGenerator->Writer()->MarkLabel(falseLabel);
            Emit(pnode->sxIf.pnodeFalse, byteCodeGenerator, funcInfo, fReturnValue);
            funcInfo->ReleaseLoc(pnode->sxIf.pnodeFalse);
            byteCodeGenerator->Writer()->MarkLabel(skipLabel);
        }
        else
        {
            byteCodeGenerator->Writer()->MarkLabel(falseLabel);
        }

        if (pnode->emitLabels)
        {
            byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
        }
        break;
    }
    case knopWhile:
        EmitLoop(pnode,
            pnode->sxWhile.pnodeCond,
            pnode->sxWhile.pnodeBody,
            nullptr,
            byteCodeGenerator,
            funcInfo,
            fReturnValue);
        break;
    // PTNODE(knopDoWhile    , "do-while"    ,None    ,While,fnopBreak|fnopContinue)
    case knopDoWhile:
        EmitLoop(pnode,
            pnode->sxWhile.pnodeCond,
            pnode->sxWhile.pnodeBody,
            nullptr,
            byteCodeGenerator,
            funcInfo,
            fReturnValue,
            true);
        break;
    // PTNODE(knopForIn      , "for in"    ,None    ,ForIn,fnopBreak|fnopContinue|fnopCleanup)
    case knopForIn:
        EmitForInOrForOf(pnode, byteCodeGenerator, funcInfo, fReturnValue);
        break;
    case knopForOf:
        EmitForInOrForOf(pnode, byteCodeGenerator, funcInfo, fReturnValue);
        break;
    // PTNODE(knopReturn     , "return"    ,None    ,Uni  ,fnopNone)
    case knopReturn:
        byteCodeGenerator->StartStatement(pnode);
        if (pnode->sxReturn.pnodeExpr != nullptr)
        {
            if (pnode->sxReturn.pnodeExpr->location == Js::Constants::NoRegister)
            {
                // No need to burn a register for the return value. If we need a temp, use R0 directly.
                pnode->sxReturn.pnodeExpr->location = ByteCodeGenerator::ReturnRegister;
            }
            Emit(pnode->sxReturn.pnodeExpr, byteCodeGenerator, funcInfo, fReturnValue);
            if (pnode->sxReturn.pnodeExpr->location != ByteCodeGenerator::ReturnRegister)
            {
                byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, ByteCodeGenerator::ReturnRegister, pnode->sxReturn.pnodeExpr->location);
            }
            funcInfo->GetParsedFunctionBody()->SetHasNoExplicitReturnValue(false);
        }
        else
        {
            byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdUndef, ByteCodeGenerator::ReturnRegister);
        }
        if (funcInfo->IsClassConstructor())
        {
            // return expr; // becomes like below:
            //
            // if (IsObject(expr)) {
            //   return expr;
            // } else if (IsBaseClassConstructor) {
            //   return this;
            // } else if (!IsUndefined(expr)) {
            //   throw TypeError;
            // }

            Js::ByteCodeLabel returnExprLabel = byteCodeGenerator->Writer()->DefineLabel();
            byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrOnObject_A, returnExprLabel, ByteCodeGenerator::ReturnRegister);

            if (funcInfo->IsBaseClassConstructor())
            {
                byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, ByteCodeGenerator::ReturnRegister, funcInfo->thisPointerRegister);
            }
            else
            {
                Js::ByteCodeLabel returnThisLabel = byteCodeGenerator->Writer()->DefineLabel();
                byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrSrEq_A, returnThisLabel, ByteCodeGenerator::ReturnRegister, funcInfo->undefinedConstantRegister);
                byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(JSERR_ClassDerivedConstructorInvalidReturnType));
                byteCodeGenerator->Writer()->MarkLabel(returnThisLabel);
                byteCodeGenerator->EmitClassConstructorEndCode(funcInfo);
            }

            byteCodeGenerator->Writer()->MarkLabel(returnExprLabel);
        }
        if (pnode->sxStmt.grfnop & fnopCleanup)
        {
            EmitJumpCleanup(pnode, nullptr, byteCodeGenerator, funcInfo);
        }

        byteCodeGenerator->Writer()->Br(funcInfo->singleExit);
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopLabel:
        break;
    // PTNODE(knopBlock      , "{}"        ,None    ,Block,fnopNone)
    case knopBlock:
        if (pnode->sxBlock.pnodeStmt != nullptr)
        {
            EmitBlock(pnode, byteCodeGenerator, funcInfo, fReturnValue);
            if (pnode->emitLabels)
            {
                byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
            }
        }
        break;
    // PTNODE(knopWith       , "with"        ,None    ,With ,fnopCleanup)
    case knopWith:
    {
        Assert(pnode->sxWith.pnodeObj != nullptr);
        byteCodeGenerator->StartStatement(pnode);
        // Copy the with object to a temp register (the location assigned to pnode) so that if the with object
        // is overwritten in the body, the lookups are not affected.
        funcInfo->AcquireLoc(pnode);
        Emit(pnode->sxWith.pnodeObj, byteCodeGenerator, funcInfo, false);

        Js::RegSlot regVal = (byteCodeGenerator->GetScriptContext()->GetConfig()->IsES6UnscopablesEnabled()) ? funcInfo->AcquireTmpRegister() : pnode->location;
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Conv_Obj, regVal, pnode->sxWith.pnodeObj->location);
        if (byteCodeGenerator->GetScriptContext()->GetConfig()->IsES6UnscopablesEnabled())
        {
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::NewWithObject, pnode->location, regVal);
        }
        byteCodeGenerator->EndStatement(pnode);

#ifdef PERF_HINT
        if (PHASE_TRACE1(Js::PerfHintPhase))
        {
            WritePerfHint(PerfHints::HasWithBlock, funcInfo->byteCodeFunction->GetFunctionBody(), byteCodeGenerator->Writer()->GetCurrentOffset() - 1);
        }
#endif
        if (pnode->sxWith.pnodeBody != nullptr)
        {
            Scope *scope = pnode->sxWith.scope;
            scope->SetLocation(pnode->location);
            byteCodeGenerator->PushScope(scope);

            Js::DebuggerScope *debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnode, Js::DiagExtraScopesType::DiagWithScope, regVal);

            if (byteCodeGenerator->ShouldTrackDebuggerMetadata())
            {
                byteCodeGenerator->Writer()->AddPropertyToDebuggerScope(debuggerScope, regVal, Js::Constants::NoProperty, /*shouldConsumeRegister*/ true, Js::DebuggerScopePropertyFlags_WithObject);
            }

            Emit(pnode->sxWith.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);
            funcInfo->ReleaseLoc(pnode->sxWith.pnodeBody);
            byteCodeGenerator->PopScope();

            byteCodeGenerator->RecordEndScopeObject(pnode);
        }
        if (pnode->emitLabels)
        {
            byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
        }
        if (byteCodeGenerator->GetScriptContext()->GetConfig()->IsES6UnscopablesEnabled())
        {
            funcInfo->ReleaseTmpRegister(regVal);
        }
        funcInfo->ReleaseLoc(pnode->sxWith.pnodeObj);
        break;
    }
    // PTNODE(knopBreak      , "break"        ,None    ,Jump ,fnopNone)
    case knopBreak:
        Assert(pnode->sxJump.pnodeTarget->emitLabels);
        byteCodeGenerator->StartStatement(pnode);
        if (pnode->sxStmt.grfnop & fnopCleanup)
        {
            EmitJumpCleanup(pnode, pnode->sxJump.pnodeTarget, byteCodeGenerator, funcInfo);
        }
        byteCodeGenerator->Writer()->Br(pnode->sxJump.pnodeTarget->sxStmt.breakLabel);
        if (pnode->emitLabels)
        {
            byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
        }
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopContinue:
        Assert(pnode->sxJump.pnodeTarget->emitLabels);
        byteCodeGenerator->StartStatement(pnode);
        if (pnode->sxStmt.grfnop & fnopCleanup)
        {
            EmitJumpCleanup(pnode, pnode->sxJump.pnodeTarget, byteCodeGenerator, funcInfo);
        }
        byteCodeGenerator->Writer()->Br(pnode->sxJump.pnodeTarget->sxStmt.continueLabel);
        byteCodeGenerator->EndStatement(pnode);
        break;
    // PTNODE(knopContinue   , "continue"    ,None    ,Jump ,fnopNone)
    case knopSwitch:
    {
        BOOL fHasDefault = false;
        Assert(pnode->sxSwitch.pnodeVal != nullptr);
        byteCodeGenerator->StartStatement(pnode);
        Emit(pnode->sxSwitch.pnodeVal, byteCodeGenerator, funcInfo, false);

        Js::RegSlot regVal = funcInfo->AcquireTmpRegister();

        byteCodeGenerator->Writer()->Reg2(Js::OpCode::BeginSwitch, regVal, pnode->sxSwitch.pnodeVal->location);

        BeginEmitBlock(pnode->sxSwitch.pnodeBlock, byteCodeGenerator, funcInfo);

        byteCodeGenerator->EndStatement(pnode);

        // TODO: if all cases are compile-time constants, emit a switch statement in the byte
        // code so the BE can optimize it.

        ParseNode *pnodeCase;
        for (pnodeCase = pnode->sxSwitch.pnodeCases; pnodeCase; pnodeCase = pnodeCase->sxCase.pnodeNext)
        {
            // Jump to the first case body if this one doesn't match. Make sure any side-effects of the case
            // expression take place regardless.
            pnodeCase->sxCase.labelCase = byteCodeGenerator->Writer()->DefineLabel();
            if (pnodeCase == pnode->sxSwitch.pnodeDefault)
            {
                fHasDefault = true;
                continue;
            }
            Emit(pnodeCase->sxCase.pnodeExpr, byteCodeGenerator, funcInfo, false);
            byteCodeGenerator->Writer()->BrReg2(
                Js::OpCode::Case, pnodeCase->sxCase.labelCase, regVal, pnodeCase->sxCase.pnodeExpr->location);
            funcInfo->ReleaseLoc(pnodeCase->sxCase.pnodeExpr);
        }

        // No explicit case value matches. Jump to the default arm (if any) or break out altogether.
        if (fHasDefault)
        {
            byteCodeGenerator->Writer()->Br(Js::OpCode::EndSwitch, pnode->sxSwitch.pnodeDefault->sxCase.labelCase);
        }
        else
        {
            if (!pnode->emitLabels)
            {
                pnode->sxStmt.breakLabel = byteCodeGenerator->Writer()->DefineLabel();
            }
            byteCodeGenerator->Writer()->Br(Js::OpCode::EndSwitch, pnode->sxStmt.breakLabel);
        }
        // Now emit the case arms to which we jump on matching a case value.
        for (pnodeCase = pnode->sxSwitch.pnodeCases; pnodeCase; pnodeCase = pnodeCase->sxCase.pnodeNext)
        {
            byteCodeGenerator->Writer()->MarkLabel(pnodeCase->sxCase.labelCase);
            Emit(pnodeCase->sxCase.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);
            funcInfo->ReleaseLoc(pnodeCase->sxCase.pnodeBody);
        }

        EndEmitBlock(pnode->sxSwitch.pnodeBlock, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseTmpRegister(regVal);
        funcInfo->ReleaseLoc(pnode->sxSwitch.pnodeVal);

        if (!fHasDefault || pnode->emitLabels)
        {
            byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
        }
        break;
    }

    case knopTryCatch:
    {
        Js::ByteCodeLabel catchLabel = (Js::ByteCodeLabel) - 1;

        ParseNode *pnodeTry = pnode->sxTryCatch.pnodeTry;
        Assert(pnodeTry);
        ParseNode *pnodeCatch = pnode->sxTryCatch.pnodeCatch;
        Assert(pnodeCatch);

        catchLabel = byteCodeGenerator->Writer()->DefineLabel();

        // Note: try uses OpCode::Leave which causes a return to parent interpreter thunk,
        // same for catch block. Thus record cross interpreter frame entry/exit records for them.
        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(/* isEnterBlock = */ true);

        byteCodeGenerator->Writer()->Br(Js::OpCode::TryCatch, catchLabel);

        ByteCodeGenerator::TryScopeRecord tryRecForTry(Js::OpCode::TryCatch, catchLabel);
        if (funcInfo->byteCodeFunction->IsGenerator())
        {
            byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForTry);
        }

        Emit(pnodeTry->sxTry.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);
        funcInfo->ReleaseLoc(pnodeTry->sxTry.pnodeBody);

        if (funcInfo->byteCodeFunction->IsGenerator())
        {
            byteCodeGenerator->tryScopeRecordsList.UnlinkFromEnd();
        }

        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(/* isEnterBlock = */ false);

        byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
        byteCodeGenerator->Writer()->Br(pnode->sxStmt.breakLabel);
        byteCodeGenerator->Writer()->MarkLabel(catchLabel);
        Assert(pnodeCatch->sxCatch.pnodeParam);
        ParseNode *pnodeObj = pnodeCatch->sxCatch.pnodeParam;
        Js::RegSlot location;

        bool acquiredTempLocation = false;

        Js::DebuggerScope *debuggerScope = nullptr;
        Js::DebuggerScopePropertyFlags debuggerPropertyFlags = Js::DebuggerScopePropertyFlags_CatchObject;

        bool isPattern = pnodeObj->nop == knopParamPattern;

        if (isPattern)
        {
            location = pnodeObj->sxParamPattern.location;
        }
        else
        {
            location = pnodeObj->sxPid.sym->GetLocation();
        }

        if (location == Js::Constants::NoRegister)
        {
            location = funcInfo->AcquireLoc(pnodeObj);
            acquiredTempLocation = true;
        }
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::Catch, location);

        Scope *scope = pnodeCatch->sxCatch.scope;

        if (isPattern || scope->GetMustInstantiate())
        {
            byteCodeGenerator->PushScope(scope);
        }

        if (scope->GetMustInstantiate())
        {
            Assert(scope->GetLocation() == Js::Constants::NoRegister);
            if (scope->GetIsObject())
            {
                debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnode, Js::DiagCatchScopeInObject, funcInfo->InnerScopeToRegSlot(scope));
                byteCodeGenerator->Writer()->Unsigned1(Js::OpCode::NewPseudoScope, scope->GetInnerScopeIndex());
            }
            else
            {

                int index = Js::DebuggerScope::InvalidScopeIndex;
                debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnode, Js::DiagCatchScopeInSlot, funcInfo->InnerScopeToRegSlot(scope), &index);

                byteCodeGenerator->Writer()->Num3(Js::OpCode::NewInnerScopeSlots, scope->GetInnerScopeIndex(), scope->GetScopeSlotCount() + Js::ScopeSlots::FirstSlotIndex, index);
            }
        }
        else
        {
            debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnode, Js::DiagCatchScopeDirect, location);
        }

        auto ParamTrackAndInitialization = [&](Symbol *sym, bool initializeParam, Js::RegSlot location)
        {
            if (sym->IsInSlot(funcInfo))
            {
                Assert(scope->GetMustInstantiate());
                if (scope->GetIsObject())
                {
                    Js::OpCode op = (sym->GetDecl()->nop == knopLetDecl) ? Js::OpCode::InitUndeclLetFld : 
                        byteCodeGenerator->GetInitFldOp(scope, scope->GetLocation(), funcInfo, false);

                    Js::PropertyId propertyId = sym->EnsurePosition(byteCodeGenerator);
                    uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->InnerScopeToRegSlot(scope), propertyId, false, true);
                    byteCodeGenerator->Writer()->ElementPIndexed(op, location, scope->GetInnerScopeIndex(), cacheId);

                    byteCodeGenerator->TrackActivationObjectPropertyForDebugger(debuggerScope, sym, debuggerPropertyFlags);
                }
                else
                {
                    byteCodeGenerator->TrackSlotArrayPropertyForDebugger(debuggerScope, sym, sym->EnsurePosition(byteCodeGenerator), debuggerPropertyFlags);
                    if (initializeParam)
                    {
                        byteCodeGenerator->EmitLocalPropInit(location, sym, funcInfo);
                    }
                    else
                    {
                        Js::RegSlot tmpReg = funcInfo->AcquireTmpRegister();
                        byteCodeGenerator->Writer()->Reg1(Js::OpCode::InitUndecl, tmpReg);
                        byteCodeGenerator->EmitLocalPropInit(tmpReg, sym, funcInfo);
                        funcInfo->ReleaseTmpRegister(tmpReg);
                    }
                }
            }
            else
            {
                byteCodeGenerator->TrackRegisterPropertyForDebugger(debuggerScope, sym, funcInfo, debuggerPropertyFlags);
                if (initializeParam)
                {
                    byteCodeGenerator->EmitLocalPropInit(location, sym, funcInfo);
                }
                else
                {
                    byteCodeGenerator->Writer()->Reg1(Js::OpCode::InitUndecl, location);
                }
            }
        };

        if (isPattern)
        {
            Parser::MapBindIdentifier(pnodeObj->sxParamPattern.pnode1, [&](ParseNodePtr item)
            {
                ParamTrackAndInitialization(item->sxVar.sym, false /*initializeParam*/, item->sxVar.sym->GetLocation());
            });
            byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(true);

            // Now emitting bytecode for destructuring pattern
            byteCodeGenerator->StartStatement(pnodeCatch);
            ParseNodePtr pnode1 = pnodeObj->sxParamPattern.pnode1;
            Assert(pnode1->IsPattern());
            EmitAssignment(nullptr, pnode1, location, byteCodeGenerator, funcInfo);
            byteCodeGenerator->EndStatement(pnodeCatch);
        }
        else
        {
            ParamTrackAndInitialization(pnodeObj->sxPid.sym, true /*initializeParam*/, location);
            if (scope->GetMustInstantiate())
            {
                pnodeObj->sxPid.sym->SetIsGlobalCatch(true);
            }
            byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(true);

            // Allow a debugger to stop on the 'catch (e)'
            byteCodeGenerator->StartStatement(pnodeCatch);
            byteCodeGenerator->Writer()->Empty(Js::OpCode::Nop);
            byteCodeGenerator->EndStatement(pnodeCatch);
        }

        ByteCodeGenerator::TryScopeRecord tryRecForCatch(Js::OpCode::ResumeCatch, catchLabel);
        if (funcInfo->byteCodeFunction->IsGenerator())
        {
            byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForCatch);
        }

        Emit(pnodeCatch->sxCatch.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);

        if (funcInfo->byteCodeFunction->IsGenerator())
        {
            byteCodeGenerator->tryScopeRecordsList.UnlinkFromEnd();
        }

        if (scope->GetMustInstantiate() || isPattern)
        {
            byteCodeGenerator->PopScope();
        }

        byteCodeGenerator->RecordEndScopeObject(pnode);

        funcInfo->ReleaseLoc(pnodeCatch->sxCatch.pnodeBody);

        if (acquiredTempLocation)
        {
            funcInfo->ReleaseLoc(pnodeObj);
        }

        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(false);

        byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
        byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
        break;
    }

    case knopTryFinally:
    {
        Js::ByteCodeLabel finallyLabel = (Js::ByteCodeLabel) - 1;

        ParseNode *pnodeTry = pnode->sxTryFinally.pnodeTry;
        Assert(pnodeTry);
        ParseNode *pnodeFinally = pnode->sxTryFinally.pnodeFinally;
        Assert(pnodeFinally);

        // If we yield from the finally block after an exception, we have to store the exception object for the future next call.
        // When we yield from the Try-Finally the offset to the end of the Try block is needed for the branch instruction.
        Js::RegSlot regException = Js::Constants::NoRegister;
        Js::RegSlot regOffset = Js::Constants::NoRegister;

        finallyLabel = byteCodeGenerator->Writer()->DefineLabel();
        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(true);

        // [CONSIDER][aneeshd] Ideally the TryFinallyWithYield opcode needs to be used only if there is a yield expression.
        // For now, if the function is generator we are using the TryFinallyWithYield.
        ByteCodeGenerator::TryScopeRecord tryRecForTry(Js::OpCode::TryFinallyWithYield, finallyLabel);
        if (funcInfo->byteCodeFunction->IsGenerator())
        {
            regException = funcInfo->AcquireTmpRegister();
            regOffset = funcInfo->AcquireTmpRegister();
            byteCodeGenerator->Writer()->BrReg2(Js::OpCode::TryFinallyWithYield, finallyLabel, regException, regOffset);
            tryRecForTry.reg1 = regException;
            tryRecForTry.reg2 = regOffset;
            byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForTry);
        }
        else
        {
            byteCodeGenerator->Writer()->Br(Js::OpCode::TryFinally, finallyLabel);
        }

        Emit(pnodeTry->sxTry.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);
        funcInfo->ReleaseLoc(pnodeTry->sxTry.pnodeBody);

        if (funcInfo->byteCodeFunction->IsGenerator())
        {
            byteCodeGenerator->tryScopeRecordsList.UnlinkFromEnd();
        }

        byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(false);

        // Note: although we don't use OpCode::Leave for finally block,
        // OpCode::LeaveNull causes a return to parent interpreter thunk.
        // This has to be on offset prior to offset of 1st statement of finally.
        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(true);

        byteCodeGenerator->Writer()->Br(pnode->sxStmt.breakLabel);
        byteCodeGenerator->Writer()->MarkLabel(finallyLabel);

        ByteCodeGenerator::TryScopeRecord tryRecForFinally(Js::OpCode::ResumeFinally, finallyLabel, regException, regOffset);
        if (funcInfo->byteCodeFunction->IsGenerator())
        {
            byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForFinally);
        }

        Emit(pnodeFinally->sxFinally.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);
        funcInfo->ReleaseLoc(pnodeFinally->sxFinally.pnodeBody);

        if (funcInfo->byteCodeFunction->IsGenerator())
        {
            byteCodeGenerator->tryScopeRecordsList.UnlinkFromEnd();
            funcInfo->ReleaseTmpRegister(regOffset);
            funcInfo->ReleaseTmpRegister(regException);
        }

        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(false);

        byteCodeGenerator->Writer()->Empty(Js::OpCode::LeaveNull);

        byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
        break;
    }
    case knopThrow:
        byteCodeGenerator->StartStatement(pnode);
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::Throw, pnode->sxUni.pnode1->location);
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopYieldLeaf:
        byteCodeGenerator->StartStatement(pnode);
        funcInfo->AcquireLoc(pnode);
        EmitYield(funcInfo->undefinedConstantRegister, pnode->location, byteCodeGenerator, funcInfo);
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopAwait:
    case knopYield:
        byteCodeGenerator->StartStatement(pnode);
        funcInfo->AcquireLoc(pnode);
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        EmitYield(pnode->sxUni.pnode1->location, pnode->location, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopYieldStar:
        byteCodeGenerator->StartStatement(pnode);
        EmitYieldStar(pnode, byteCodeGenerator, funcInfo);
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopAsyncSpawn:
        EmitBinary(Js::OpCode::AsyncSpawn, pnode, byteCodeGenerator, funcInfo);
        break;
    default:
        AssertMsg(0, "emit unhandled pnode op");
        break;
    }

    if (fReturnValue && IsExpressionStatement(pnode, byteCodeGenerator->GetScriptContext()))
    {
        // If this statement may produce the global function's return value, copy its result to the return register.
        // fReturnValue implies global function, which implies that "return" is a parse error.
        Assert(funcInfo->IsGlobalFunction());
        Assert(pnode->nop != knopReturn);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, ByteCodeGenerator::ReturnRegister, pnode->location);
    }
}

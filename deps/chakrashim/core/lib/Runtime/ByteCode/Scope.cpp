//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

bool Scope::IsGlobalEvalBlockScope() const
{
    return this->scopeType == ScopeType_GlobalEvalBlock;
}

bool Scope::IsBlockScope(FuncInfo *funcInfo)
{
    return this != funcInfo->GetBodyScope() && this != funcInfo->GetParamScope();
}

void Scope::SetHasLocalInClosure(bool has)
{
    // (Note: if any catch var is closure-captured, we won't merge the catch scope with the function scope.
    // So don't mark the function scope "has local in closure".)
    if (has && (this == func->GetBodyScope() || this == func->GetParamScope()) || (GetCanMerge() && (this->scopeType != ScopeType_Catch && this->scopeType != ScopeType_CatchParamPattern)))
    {
        func->SetHasLocalInClosure(true);
    }
    else
    {
        if (hasCrossScopeFuncAssignment)
        {
            func->SetHasMaybeEscapedNestedFunc(DebugOnly(L"InstantiateScopeWithCrossScopeAssignment"));
        }
        SetMustInstantiate(true);
    }
}

int Scope::AddScopeSlot()
{
    int slot = scopeSlotCount++;
    if (scopeSlotCount == Js::ScopeSlots::MaxEncodedSlotCount)
    {
        this->GetEnclosingFunc()->SetHasMaybeEscapedNestedFunc(DebugOnly(L"TooManySlots"));
    }
    return slot;
}

void Scope::ForceAllSymbolNonLocalReference(ByteCodeGenerator *byteCodeGenerator)
{
    this->ForEachSymbol([this, byteCodeGenerator](Symbol *const sym)
    {
        if (!sym->GetIsArguments())
        {
            sym->SetHasNonLocalReference(true, byteCodeGenerator);
            this->GetFunc()->SetHasLocalInClosure(true);
        }
    });
}

bool Scope::IsEmpty() const
{
    if (GetFunc()->bodyScope == this || (GetFunc()->IsGlobalFunction() && this->IsGlobalEvalBlockScope()))
    {
        return Count() == 0 && !GetFunc()->isThisLexicallyCaptured;
    }
    else
    {
        return Count() == 0;
    }
}

void Scope::SetIsObject()
{
    if (this->isObject)
    {
        return;
    }

    this->isObject = true;

    // We might set the scope to be object after we have process the symbol
    // (e.g. "With" scope referencing a symbol in an outer scope).
    // If we have func assignment, we need to mark the function to not do stack nested function
    // as these are now assigned to a scope object.
    FuncInfo * funcInfo = this->GetFunc();
    if (funcInfo && !funcInfo->HasMaybeEscapedNestedFunc())
    {
        this->ForEachSymbolUntil([funcInfo](Symbol * const sym)
        {
            if (sym->GetHasFuncAssignment())
            {
                funcInfo->SetHasMaybeEscapedNestedFunc(DebugOnly(L"DelayedObjectScopeAssignment"));
                return true;
            }
            return false;
        });
    }
}

void Scope::MergeParamAndBodyScopes(ParseNode *pnodeScope, ByteCodeGenerator *byteCodeGenerator)
{
    Assert(pnodeScope->sxFnc.funcInfo);
    Scope *paramScope = pnodeScope->sxFnc.pnodeScopes->sxBlock.scope;
    Scope *bodyScope = pnodeScope->sxFnc.pnodeBodyScope->sxBlock.scope;

    Assert(paramScope->m_symList == nullptr || paramScope->symbolTable == nullptr);
    Assert(bodyScope->m_symList == nullptr || bodyScope->symbolTable == nullptr);

    if (paramScope->Count() == 0)
    {
        // Once the scopes are merged, there's no reason to instantiate the param scope.
        paramScope->SetMustInstantiate(false);

        // Scopes are already merged or we don't have an arguments object. Go ahead and
        // remove the param scope from the scope chain.
        bodyScope->SetEnclosingScope(paramScope->GetEnclosingScope());
        return;
    }

    bodyScope->ForEachSymbol([&](Symbol * sym)
    {
        // Duplicate 'arguments' - param scope arguments wins.
        if (byteCodeGenerator->UseParserBindings()
            && sym->GetDecl()->sxVar.pid == byteCodeGenerator->GetParser()->names()->arguments)
        {
            return;
        }

        Assert(paramScope->m_symList == nullptr || paramScope->FindLocalSymbol(sym->GetName()) == nullptr);
        paramScope->AddNewSymbol(sym);
    });

    // Reassign non-formal slot positions. Formals need to keep their slot positions to ensure
    // the argument object works properly. Other symbols need to be reassigned slot positions.
    paramScope->ForEachSymbol([&](Symbol * sym)
    {
        if (sym->GetSymbolType() != STFormal && sym->GetScopeSlot() != Js::Constants::NoProperty)
        {
            sym->SetScopeSlot(Js::Constants::NoProperty);
            sym->EnsureScopeSlot(pnodeScope->sxFnc.funcInfo);
        }
        sym->SetScope(bodyScope);
    });

    bodyScope->m_count = paramScope->m_count;
    bodyScope->m_symList = paramScope->m_symList;
    bodyScope->scopeSlotCount = paramScope->scopeSlotCount;
    if (bodyScope->symbolTable != nullptr)
    {
        Adelete(byteCodeGenerator->GetAllocator(), bodyScope->symbolTable);
        bodyScope->symbolTable = nullptr;
    }
    bodyScope->symbolTable = paramScope->symbolTable;
    if (paramScope->GetIsObject())
    {
        bodyScope->SetIsObject();
    }
    if (paramScope->GetMustInstantiate())
    {
        bodyScope->SetMustInstantiate(true);
    }

    // Once the scopes are merged, there's no reason to instantiate the param scope.
    paramScope->SetMustInstantiate(false);

    paramScope->m_count = 0;
    paramScope->scopeSlotCount = 0;
    paramScope->m_symList = nullptr;
    paramScope->symbolTable = nullptr;

    // Remove the parameter scope from the scope chain.
    bodyScope->SetEnclosingScope(paramScope->GetEnclosingScope());
}

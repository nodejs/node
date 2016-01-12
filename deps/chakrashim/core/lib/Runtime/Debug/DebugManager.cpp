//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"
#include "Language\JavascriptStackWalker.h"
namespace Js
{
    DebugManager::DebugManager(ThreadContext* _pThreadContext, AllocationPolicyManager * allocationPolicyManager) :
        pCurrentInterpreterLocation(nullptr),
        secondaryCurrentSourceContext(0),
        debugSessionNumber(0),
        pThreadContext(_pThreadContext),
        isAtDispatchHalt(false),
        mutationNewValuePid(Js::Constants::NoProperty),
        mutationPropertyNamePid(Js::Constants::NoProperty),
        mutationTypePid(Js::Constants::NoProperty),
        diagnosticPageAllocator(allocationPolicyManager, Js::Configuration::Global.flags, PageAllocatorType_Diag, 0),
        evalCodeRegistrationCount(0),
        anonymousCodeRegistrationCount(0),
        jscriptBlockRegistrationCount(0),
        isDebuggerAttaching(false)
    {
        Assert(_pThreadContext != nullptr);
#if DBG
        dispatchHaltFrameAddress = nullptr;
        // diagnosticPageAllocator may be used in multiple thread, but it's usage is synchronized.
        diagnosticPageAllocator.SetDisableThreadAccessCheck();
        diagnosticPageAllocator.debugName = L"Diagnostic";
#endif
    }

    void DebugManager::Close()
    {
        this->diagnosticPageAllocator.Close();

        if (this->pConsoleScope)
        {
            this->pConsoleScope.Unroot(this->pThreadContext->GetRecycler());
        }
#if DBG
        this->pThreadContext->EnsureNoReturnedValueList();
#endif
        this->pThreadContext = nullptr;
    }

    DebugManager::~DebugManager()
    {
        Assert(this->pThreadContext == nullptr);
    }

    DebuggingFlags* DebugManager::GetDebuggingFlags()
    {
        return &this->debuggingFlags;
    }

    ReferencedArenaAdapter* DebugManager::GetDiagnosticArena()
    {
        if (pCurrentInterpreterLocation)
        {
            return pCurrentInterpreterLocation->referencedDiagnosticArena;
        }
        return nullptr;
    }

    DWORD_PTR DebugManager::AllocateSecondaryHostSourceContext()
    {
        Assert(secondaryCurrentSourceContext < ULONG_MAX);
        return secondaryCurrentSourceContext++; // The context is not valid, use the secondary context for identify the function body for further use.
    }

    void DebugManager::SetCurrentInterpreterLocation(InterpreterHaltState* pHaltState)
    {
        Assert(pHaltState);
        Assert(!pCurrentInterpreterLocation);

        pCurrentInterpreterLocation = pHaltState;

        AutoAllocatorObjectPtr<ArenaAllocator, HeapAllocator> pDiagArena(HeapNew(ArenaAllocator, L"DiagHaltState", this->pThreadContext->GetPageAllocator(), Js::Throw::OutOfMemory), &HeapAllocator::Instance);
        AutoAllocatorObjectPtr<ReferencedArenaAdapter, HeapAllocator> referencedDiagnosticArena(HeapNew(ReferencedArenaAdapter, pDiagArena), &HeapAllocator::Instance);
        pCurrentInterpreterLocation->referencedDiagnosticArena = referencedDiagnosticArena;

        pThreadContext->GetRecycler()->RegisterExternalGuestArena(pDiagArena);
        debugSessionNumber++;

        pDiagArena.Detach();
        referencedDiagnosticArena.Detach();
    }

    void DebugManager::UnsetCurrentInterpreterLocation()
    {
        Assert(pCurrentInterpreterLocation);

        if (pCurrentInterpreterLocation)
        {
            // pCurrentInterpreterLocation->referencedDiagnosticArena could be null if we ran out of memory during SetCurrentInterpreterLocation
            if (pCurrentInterpreterLocation->referencedDiagnosticArena)
            {
                pThreadContext->GetRecycler()->UnregisterExternalGuestArena(pCurrentInterpreterLocation->referencedDiagnosticArena->Arena());
                pCurrentInterpreterLocation->referencedDiagnosticArena->DeleteArena();
                pCurrentInterpreterLocation->referencedDiagnosticArena->Release();
            }

            pCurrentInterpreterLocation = nullptr;
        }
    }

#ifdef ENABLE_MUTATION_BREAKPOINT
    MutationBreakpoint* DebugManager::GetActiveMutationBreakpoint() const
    {
        Assert(this->pCurrentInterpreterLocation);
        return this->pCurrentInterpreterLocation->activeMutationBP;
    }
#endif

    DynamicObject* DebugManager::GetConsoleScope(ScriptContext* scriptContext)
    {
        Assert(scriptContext);

        if (!this->pConsoleScope)
        {
            this->pConsoleScope.Root(scriptContext->GetLibrary()->CreateConsoleScopeActivationObject(), this->pThreadContext->GetRecycler());
        }

        return (DynamicObject*)CrossSite::MarshalVar(scriptContext, (Var)this->pConsoleScope);
    }

    FrameDisplay *DebugManager::GetFrameDisplay(ScriptContext* scriptContext, DynamicObject* scopeAtZero, DynamicObject* scopeAtOne, bool addGlobalThisAtScopeTwo)
    {
        // The scope chain for console eval looks like:
        //  - dummy empty object - new vars, let, consts, functions get added here
        //  - Active scope object containing all globals visible at this break (if at break)
        //  - Global this object so that existing properties are updated here
        //  - Console-1 Scope - all new globals will go here (like x = 1;)
        //  - NullFrameDisplay

        FrameDisplay* environment = JavascriptOperators::OP_LdFrameDisplay(this->GetConsoleScope(scriptContext), const_cast<FrameDisplay *>(&NullFrameDisplay), scriptContext);

        if (addGlobalThisAtScopeTwo)
        {
            environment = JavascriptOperators::OP_LdFrameDisplay(scriptContext->GetGlobalObject()->ToThis(), environment, scriptContext);
        }

        if (scopeAtOne != nullptr)
        {
            environment = JavascriptOperators::OP_LdFrameDisplay((Var)scopeAtOne, environment, scriptContext);
        }

        environment = JavascriptOperators::OP_LdFrameDisplay((Var)scopeAtZero, environment, scriptContext);
        return environment;
    }

    void DebugManager::UpdateConsoleScope(DynamicObject* copyFromScope, ScriptContext* scriptContext)
    {
        Assert(copyFromScope != nullptr);
        DynamicObject* consoleScope = this->GetConsoleScope(scriptContext);
        Js::RecyclableObject* recyclableObject = Js::RecyclableObject::FromVar(copyFromScope);

        ulong newPropCount = recyclableObject->GetPropertyCount();
        for (ulong i = 0; i < newPropCount; i++)
        {
            Js::PropertyId propertyId = recyclableObject->GetPropertyId((Js::PropertyIndex)i);
            // For deleted properties we won't have a property id
            if (propertyId != Js::Constants::NoProperty)
            {
                Js::PropertyValueInfo propertyValueInfo;
                Var propertyValue;
                BOOL gotPropertyValue = recyclableObject->GetProperty(recyclableObject, propertyId, &propertyValue, &propertyValueInfo, scriptContext);
                AssertMsg(gotPropertyValue, "DebugManager::UpdateConsoleScope Should have got valid value?");

                OUTPUT_TRACE(Js::ConsoleScopePhase, L"Adding property '%s'\n", scriptContext->GetPropertyName(propertyId)->GetBuffer());

                BOOL updateSuccess = consoleScope->SetPropertyWithAttributes(propertyId, propertyValue, propertyValueInfo.GetAttributes(), &propertyValueInfo);
                AssertMsg(updateSuccess, "DebugManager::UpdateConsoleScope Unable to update property value. Am I missing a scenario?");
            }
        }

        OUTPUT_TRACE(Js::ConsoleScopePhase, L"Number of properties on console scope object after update are %d\n", consoleScope->GetPropertyCount());
    }

#if DBG
    void DebugManager::ValidateDebugAPICall()
    {
        Js::JavascriptStackWalker walker(this->pThreadContext->GetScriptEntryExit()->scriptContext);
        Js::JavascriptFunction* javascriptFunction = nullptr;
        if (walker.GetCaller(&javascriptFunction))
        {
            if (javascriptFunction != nullptr)
            {
                void *topJsFrameAddr = (void *)walker.GetCurrentArgv();
                Assert(this->dispatchHaltFrameAddress != nullptr);
                if (topJsFrameAddr < this->dispatchHaltFrameAddress)
                {
                    // we found the script frame after the break mode.
                    AssertMsg(false, "There are JavaScript frames between current API and dispatch halt");
                }
            }
        }
    }
#endif
}

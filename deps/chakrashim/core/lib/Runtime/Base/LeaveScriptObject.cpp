//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

namespace Js
{
    EnterScriptObject::EnterScriptObject(ScriptContext* scriptContext, ScriptEntryExitRecord* entryExitRecord,
        void * returnAddress, bool doCleanup, bool isCallRoot, bool hasCaller)
    {
        Assert(scriptContext);

#ifdef PROFILE_EXEC
        scriptContext->ProfileBegin(Js::RunPhase);
#endif

        if (scriptContext->GetThreadContext() &&
            scriptContext->GetThreadContext()->IsNoScriptScope())
        {
            FromDOM_NoScriptScope_fatal_error();
        }

        // Keep a copy locally so the optimizer can just copy prop it to the dtor
        this->scriptContext = scriptContext;
        this->entryExitRecord = entryExitRecord;
        this->doCleanup = doCleanup;
        this->isCallRoot = isCallRoot;
        this->hr = NOERROR;
        this->hasForcedEnter = scriptContext->GetDebugContext() != nullptr ? scriptContext->GetDebugContext()->GetProbeContainer()->isForcedToEnterScriptStart : false;

        // Initialize the entry exit record
        entryExitRecord->returnAddrOfScriptEntryFunction = returnAddress;
        entryExitRecord->hasCaller = hasCaller;
        entryExitRecord->scriptContext = scriptContext;
#ifdef EXCEPTION_CHECK
        entryExitRecord->handledExceptionType = ExceptionCheck::ClearHandledExceptionType();
#endif
#if DBG_DUMP
        entryExitRecord->isCallRoot = isCallRoot;
#endif
        if (!scriptContext->IsClosed())
        {
            library = scriptContext->GetLibrary();
        }
        try
        {
            AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_OutOfMemory);
            scriptContext->GetThreadContext()->PushHostScriptContext(scriptContext->GetHostScriptContext());
        }
        catch (Js::OutOfMemoryException)
        {
            this->hr = E_OUTOFMEMORY;
        }
        BEGIN_NO_EXCEPTION
        {
            // We can not have any exception in the constructor, otherwise the destructor will
            // not run and we might be in an inconsistent state

            // Put any code that may raise an exception in OnScriptStart
            scriptContext->GetThreadContext()->EnterScriptStart(entryExitRecord, doCleanup);
        }
            END_NO_EXCEPTION
    }

    void EnterScriptObject::VerifyEnterScript()
    {
        if (FAILED(hr))
        {
            Assert(hr == E_OUTOFMEMORY);
            throw Js::OutOfMemoryException();
        }
    }

    EnterScriptObject::~EnterScriptObject()
    {
        scriptContext->OnScriptEnd(isCallRoot, hasForcedEnter);
        if (SUCCEEDED(hr))
        {
            scriptContext->GetThreadContext()->PopHostScriptContext();
        }
        scriptContext->GetThreadContext()->EnterScriptEnd(entryExitRecord, doCleanup);
#ifdef PROFILE_EXEC
        scriptContext->ProfileEnd(Js::RunPhase);
#endif
    }
};

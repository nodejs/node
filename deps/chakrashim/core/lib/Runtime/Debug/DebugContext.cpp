//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

namespace Js
{
    DebugContext::DebugContext(Js::ScriptContext * scriptContext) :
        scriptContext(scriptContext),
        hostDebugContext(nullptr),
        diagProbesContainer(nullptr),
        debuggerMode(DebuggerMode::NotDebugging)
    {
        Assert(scriptContext != nullptr);
    }

    DebugContext::~DebugContext()
    {
        Assert(this->scriptContext == nullptr);
        Assert(this->hostDebugContext == nullptr);
        Assert(this->diagProbesContainer == nullptr);
    }

    void DebugContext::Initialize()
    {
        Assert(this->diagProbesContainer == nullptr);
        this->diagProbesContainer = HeapNew(ProbeContainer);
        this->diagProbesContainer->Initialize(this->scriptContext);
    }

    void DebugContext::Close()
    {
        Assert(this->scriptContext != nullptr);
        this->scriptContext = nullptr;

        if (this->diagProbesContainer != nullptr)
        {
            this->diagProbesContainer->Close();
            HeapDelete(this->diagProbesContainer);
            this->diagProbesContainer = nullptr;
        }

        if (this->hostDebugContext != nullptr)
        {
            this->hostDebugContext->Delete();
            this->hostDebugContext = nullptr;
        }
    }

    void DebugContext::SetHostDebugContext(HostDebugContext * hostDebugContext)
    {
        Assert(this->hostDebugContext == nullptr);
        Assert(hostDebugContext != nullptr);

        this->hostDebugContext = hostDebugContext;
    }

    bool DebugContext::CanRegisterFunction() const
    {
        if (this->hostDebugContext == nullptr || this->scriptContext == nullptr || this->scriptContext->IsClosed() || this->IsInNonDebugMode())
        {
            return false;
        }
        return true;
    }

    void DebugContext::RegisterFunction(Js::ParseableFunctionInfo * func, LPCWSTR title)
    {
        if (!this->CanRegisterFunction())
        {
            return;
        }

        FunctionBody * functionBody;
        if (func->IsDeferredParseFunction())
        {
            functionBody = func->Parse();
        }
        else
        {
            functionBody = func->GetFunctionBody();
        }
        this->RegisterFunction(functionBody, functionBody->GetHostSourceContext(), title);
    }

    void DebugContext::RegisterFunction(Js::FunctionBody * functionBody, DWORD_PTR dwDebugSourceContext, LPCWSTR title)
    {
        if (!this->CanRegisterFunction())
        {
            return;
        }

        this->hostDebugContext->DbgRegisterFunction(this->scriptContext, functionBody, dwDebugSourceContext, title);
    }

    // Sets the specified mode for the debugger.  The mode is used to inform
    // the runtime of whether or not functions should be JITed or interpreted
    // when they are defer parsed.
    // Note: Transitions back to NotDebugging are not allowed.  Once the debugger
    // is in SourceRundown or Debugging mode, it can only transition between those
    // two modes.
    void DebugContext::SetDebuggerMode(DebuggerMode mode)
    {
        if (this->debuggerMode == mode)
        {
            // Already in this mode so return.
            return;
        }

        if (mode == DebuggerMode::NotDebugging)
        {
            AssertMsg(false, "Transitioning to non-debug mode is not allowed.");
            return;
        }

        this->debuggerMode = mode;
    }

    HRESULT DebugContext::RundownSourcesAndReparse(bool shouldPerformSourceRundown, bool shouldReparseFunctions)
    {
        OUTPUT_TRACE(Js::DebuggerPhase, L"DebugContext::RundownSourcesAndReparse scriptContext 0x%p, shouldPerformSourceRundown %d, shouldReparseFunctions %d\n",
            this->scriptContext, shouldPerformSourceRundown, shouldReparseFunctions);

        Js::TempArenaAllocatorObject *tempAllocator = nullptr;
        JsUtil::List<Js::FunctionBody *, ArenaAllocator>* pFunctionsToRegister = nullptr;
        JsUtil::List<Js::Utf8SourceInfo *, Recycler, false, Js::CopyRemovePolicy, RecyclerPointerComparer>* utf8SourceInfoList = nullptr;

        HRESULT hr = S_OK;
        ThreadContext* threadContext = this->scriptContext->GetThreadContext();

        BEGIN_TRANSLATE_OOM_TO_HRESULT_NESTED
        tempAllocator = threadContext->GetTemporaryAllocator(L"debuggerAlloc");

        pFunctionsToRegister = JsUtil::List<Js::FunctionBody*, ArenaAllocator>::New(tempAllocator->GetAllocator());
        utf8SourceInfoList = JsUtil::List<Js::Utf8SourceInfo *, Recycler, false, Js::CopyRemovePolicy, RecyclerPointerComparer>::New(this->scriptContext->GetRecycler());

        this->MapUTF8SourceInfoUntil([&](Js::Utf8SourceInfo * sourceInfo) -> bool
        {
            WalkAndAddUtf8SourceInfo(sourceInfo, utf8SourceInfoList);
            return false;
        });
        END_TRANSLATE_OOM_TO_HRESULT(hr);

        if (hr != S_OK)
        {
            Assert(FALSE);
            return hr;
        }

        utf8SourceInfoList->MapUntil([&](int index, Js::Utf8SourceInfo * sourceInfo) -> bool
        {
            OUTPUT_TRACE(Js::DebuggerPhase, L"DebugContext::RundownSourcesAndReparse scriptContext 0x%p, sourceInfo 0x%p, HasDebugDocument %d\n",
                this->scriptContext, sourceInfo, sourceInfo->HasDebugDocument());

            if (sourceInfo->GetIsLibraryCode())
            {
                // Not putting the internal library code to the debug mode, but need to reinitialize execution mode limits of each
                // function body upon debugger detach, even for library code at the moment.
                if (shouldReparseFunctions)
                {
                    sourceInfo->MapFunction([](Js::FunctionBody *const pFuncBody)
                    {
                        if (pFuncBody->IsFunctionParsed())
                        {
                            pFuncBody->ReinitializeExecutionModeAndLimits();
                        }
                    });
                }
                return false;
            }

            Assert(sourceInfo->GetSrcInfo() && sourceInfo->GetSrcInfo()->sourceContextInfo);

#if DBG
            if (shouldPerformSourceRundown)
            {
                // We shouldn't have a debug document if we're running source rundown for the first time.
                Assert(!sourceInfo->HasDebugDocument());
            }
#endif // DBG

            DWORD_PTR dwDebugHostSourceContext = Js::Constants::NoHostSourceContext;

            if (shouldPerformSourceRundown && this->hostDebugContext != nullptr)
            {
                dwDebugHostSourceContext = this->hostDebugContext->GetHostSourceContext(sourceInfo);
            }

            this->FetchTopLevelFunction(pFunctionsToRegister, sourceInfo);

            if (pFunctionsToRegister->Count() == 0)
            {
                // This could happen if there are no functions to re-compile.
                return false;
            }

            if (this->hostDebugContext != nullptr && sourceInfo->GetSourceContextInfo())
            {
                this->hostDebugContext->SetThreadDescription(sourceInfo->GetSourceContextInfo()->url); // the HRESULT is omitted.
            }

            bool fHasDoneSourceRundown = false;
            for (int i = 0; i < pFunctionsToRegister->Count(); i++)
            {
                Js::FunctionBody* pFuncBody = pFunctionsToRegister->Item(i);
                if (pFuncBody == nullptr)
                {
                    continue;
                }

                if (shouldReparseFunctions)
                {
                    if (this->scriptContext == nullptr || this->scriptContext->IsClosed())
                    {
                        // scriptContext can be closed in previous call
                        hr = E_FAIL;
                        return true;
                    }

                    BEGIN_JS_RUNTIME_CALL_EX_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NESTED(this->scriptContext, false)
                    {
                        pFuncBody->Parse();
                        // This is the first call to the function, ensure dynamic profile info
#if ENABLE_PROFILE_INFO
                        pFuncBody->EnsureDynamicProfileInfo();
#endif
                    }
                    END_JS_RUNTIME_CALL_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT(hr);

                    if (hr != S_OK)
                    {
                        break;
                    }
                }

                if (!fHasDoneSourceRundown && shouldPerformSourceRundown)
                {
                    BEGIN_TRANSLATE_OOM_TO_HRESULT_NESTED
                    {
                        this->RegisterFunction(pFuncBody, dwDebugHostSourceContext, pFuncBody->GetSourceName());
                    }
                    END_TRANSLATE_OOM_TO_HRESULT(hr);

                    fHasDoneSourceRundown = true;
                }
            }

            if (shouldReparseFunctions)
            {
                sourceInfo->MapFunction([](Js::FunctionBody *const pFuncBody)
                {
                    if (pFuncBody->IsFunctionParsed())
                    {
                        pFuncBody->ReinitializeExecutionModeAndLimits();
                    }
                });
            }

            return false;
        });

        if (this->scriptContext != nullptr && !this->scriptContext->IsClosed())
        {
            if (shouldPerformSourceRundown && this->scriptContext->HaveCalleeSources())
            {
                this->scriptContext->MapCalleeSources([=](Js::Utf8SourceInfo* calleeSourceInfo)
                {
                    if (this->hostDebugContext != nullptr)
                    {
                        this->hostDebugContext->ReParentToCaller(calleeSourceInfo);
                    }
                });
            }
        }
        threadContext->ReleaseTemporaryAllocator(tempAllocator);

        return hr;
    }

    void DebugContext::FetchTopLevelFunction(JsUtil::List<Js::FunctionBody *, ArenaAllocator>* pFunctions, Js::Utf8SourceInfo * sourceInfo)
    {
        Assert(pFunctions != nullptr);
        Assert(sourceInfo != nullptr);

        HRESULT hr = S_OK;

        // Get FunctionBodys which are distinctly parseable, i.e. they are not enclosed in any other function (finding
        // out root node of the sub-tree, in which root node is not enclosed in any other available function) this is
        // by walking over all function and comparing their range.

        BEGIN_TRANSLATE_OOM_TO_HRESULT_NESTED
        {
            pFunctions->Clear();

            sourceInfo->MapFunctionUntil([&](Js::FunctionBody* pFuncBody) -> bool
            {
                if (pFuncBody->GetIsGlobalFunc())
                {
                    if (pFuncBody->IsFakeGlobalFunc(pFuncBody->GetGrfscr()))
                    {
                        // This is created due to 'Function' code or deferred parsed functions, there is nothing to
                        // re-compile in this function as this is just a place-holder/fake function.

                        Assert(pFuncBody->GetByteCode() == NULL);

                        return false;
                    }

                    if (!pFuncBody->GetIsTopLevel())
                    {
                        return false;
                    }

                    // If global function, there is no need to find out any other functions.

                    pFunctions->Clear();
                    pFunctions->Add(pFuncBody);
                    return true;
                }

                if (pFuncBody->IsFunctionParsed())
                {
                    bool isNeedToAdd = true;
                    for (int i = 0; i < pFunctions->Count(); i++)
                    {
                        Js::FunctionBody *currentFunction = pFunctions->Item(i);
                        if (currentFunction != nullptr)
                        {
                            if (currentFunction->StartInDocument() > pFuncBody->StartInDocument() || !currentFunction->EndsAfter(pFuncBody->StartInDocument()))
                            {
                                if (pFuncBody->StartInDocument() <= currentFunction->StartInDocument() && pFuncBody->EndsAfter(currentFunction->StartInDocument()))
                                {
                                    // The stored item has got the parent, remove current Item
                                    pFunctions->Item(i, nullptr);
                                }
                            }
                            else
                            {
                                // Parent (the enclosing function) is already in the list
                                isNeedToAdd = false;
                                break;
                            }
                        }
                    }

                    if (isNeedToAdd)
                    {
                        pFunctions->Add(pFuncBody);
                    }
                }
                return false;
            });
        }
        END_TRANSLATE_OOM_TO_HRESULT(hr);

        Assert(hr == S_OK);
    }

    // Create an ordered flat list of sources to reparse. Caller of a source should be added to the list before we add the source itself.
    void DebugContext::WalkAndAddUtf8SourceInfo(Js::Utf8SourceInfo* sourceInfo, JsUtil::List<Js::Utf8SourceInfo *, Recycler, false, Js::CopyRemovePolicy, RecyclerPointerComparer> *utf8SourceInfoList)
    {
        Js::Utf8SourceInfo* callerUtf8SourceInfo = sourceInfo->GetCallerUtf8SourceInfo();
        if (callerUtf8SourceInfo)
        {
            Js::ScriptContext* callerScriptContext = callerUtf8SourceInfo->GetScriptContext();
            OUTPUT_TRACE(Js::DebuggerPhase, L"DebugContext::WalkAndAddUtf8SourceInfo scriptContext 0x%p, sourceInfo 0x%p, callerUtf8SourceInfo 0x%p, sourceInfo scriptContext 0x%p, callerUtf8SourceInfo scriptContext 0x%p\n",
                this->scriptContext, sourceInfo, callerUtf8SourceInfo, sourceInfo->GetScriptContext(), callerScriptContext);

            if (sourceInfo->GetScriptContext() == callerScriptContext)
            {
                WalkAndAddUtf8SourceInfo(callerUtf8SourceInfo, utf8SourceInfoList);
            }
            else if (!callerScriptContext->IsInDebugOrSourceRundownMode())
            {
                // The caller scriptContext is not in run down/debug mode so let's save the relationship so that we can re-parent callees afterwards.
                callerScriptContext->AddCalleeSourceInfoToList(sourceInfo);
            }
        }
        if (!utf8SourceInfoList->Contains(sourceInfo))
        {
            OUTPUT_TRACE(Js::DebuggerPhase, L"DebugContext::WalkAndAddUtf8SourceInfo Adding to utf8SourceInfoList scriptContext 0x%p, sourceInfo 0x%p, sourceInfo scriptContext 0x%p\n",
                this->scriptContext, sourceInfo, sourceInfo->GetScriptContext());
#if DBG
            bool found = false;
            this->MapUTF8SourceInfoUntil([&](Js::Utf8SourceInfo * sourceInfoTemp) -> bool
            {
                if (sourceInfoTemp == sourceInfo)
                {
                    found = true;
                }
                return found;
            });
            AssertMsg(found, "Parented eval feature have extra source");
#endif
            utf8SourceInfoList->Add(sourceInfo);
        }
    }

    template<class TMapFunction>
    void DebugContext::MapUTF8SourceInfoUntil(TMapFunction map)
    {
        this->scriptContext->GetSourceList()->MapUntil([=](int i, RecyclerWeakReference<Js::Utf8SourceInfo>* sourceInfoWeakRef) -> bool {
            Js::Utf8SourceInfo* sourceInfo = sourceInfoWeakRef->Get();
            if (sourceInfo)
            {
                return map(sourceInfo);
            }
            return false;
        });
    }
}

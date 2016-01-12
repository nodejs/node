//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"
#include "Library\StackScriptFunction.h"

namespace Js
{
    void JavascriptExceptionObject::FillError(Js::JavascriptExceptionContext& exceptionContext, ScriptContext *scriptContext, HostWrapperCreateFuncType hostWrapperCreateFunc)
    {
        this->scriptContext  = scriptContext;
        this->exceptionContext = exceptionContext;
        this->SetHostWrapperCreateFunc(hostWrapperCreateFunc);
    }

    void JavascriptExceptionObject::ClearError()
    {
        Assert(this->isPendingExceptionObject);
        memset(this, 0, sizeof(JavascriptExceptionObject));
        this->isPendingExceptionObject = true;
    }

    JavascriptExceptionObject* JavascriptExceptionObject::CloneIfStaticExceptionObject(ScriptContext* scriptContext)
    {
        Assert(scriptContext);

        ThreadContext *threadContext = scriptContext->GetThreadContext();
        JavascriptExceptionObject* exceptionObject = this;

        if (this == threadContext->GetPendingOOMErrorObject())
        {
            AssertMsg(this->thrownObject == NULL, "ThrownObject should be NULL since at time of OOM we will not be able to allocate the thrownObject");

            // Let's hope that unwinding has released enough pointers that the
            // recycler will find some memory to allocate the real OutOfMemory object.
            // If not, it will rethrow outOfMemory
            Var thrownObject = scriptContext->GetLibrary()->CreateOutOfMemoryError();
            exceptionObject = RecyclerNew(scriptContext->GetRecycler(),
                JavascriptExceptionObject,
                thrownObject,
                scriptContext,
                &this->exceptionContext);
            threadContext->ClearPendingOOMError();
        }

        if (this == threadContext->GetPendingSOErrorObject())
        {
            Var thrownObject = NULL;

            if (this->thrownObject == NULL)
            {
                AssertMsg(!threadContext->GetIsThreadBound(), "ThrownObject could be NULL for Jsrt scenarios because it is cleared in ~EnterScriptEnd. For non-jsrt cases, we should always have an allocated thrown object.");
                thrownObject = scriptContext->GetLibrary()->CreateStackOverflowError();
            }
            else
            {
                thrownObject = this->GetThrownObject(scriptContext);
            }

            exceptionObject = RecyclerNew(scriptContext->GetRecycler(),
                JavascriptExceptionObject,
                thrownObject,
                scriptContext,
                &this->exceptionContext);
            threadContext->ClearPendingSOError();
        }

        return exceptionObject;
    }

    // Returns NULL if the exception object is the static out of memory object.
    Var JavascriptExceptionObject::GetThrownObject(ScriptContext * requestingScriptContext)
    {
        // requestingScriptContext == this->scriptContext when we have A->(cross site thunk)B->(IDispatch)A using and nested A window return
        // exception backup. we can go back down to normal code path below.
        if (requestingScriptContext != nullptr && hostWrapperCreateFunc != nullptr && (requestingScriptContext != this->scriptContext))
        {
            return hostWrapperCreateFunc(thrownObject, scriptContext, requestingScriptContext);
        }
        // We can have cross script context throw in both fastDOM and IE8 mode now.
        if (requestingScriptContext && (thrownObject != nullptr))
        {
            Var rethrownObject = CrossSite::MarshalVar(requestingScriptContext, thrownObject);
            // For now, there is no known host for which we need to support cross-domain
            // scenario for JSRT. So skip the cross domain check for now.
            if (!(scriptContext->GetThreadContext()->GetIsThreadBound()))
            {
                return rethrownObject;
            }
            if (rethrownObject)
            {
                if (JavascriptError::Is(rethrownObject))
                {

                    JavascriptError* jsErrorObject = JavascriptError::FromVar(rethrownObject);
                    if (jsErrorObject->GetScriptContext() != requestingScriptContext )
                    {
                        Assert(requestingScriptContext->GetHostScriptContext());
                        HRESULT hr = requestingScriptContext->GetHostScriptContext()->CheckCrossDomainScriptContext(jsErrorObject->GetScriptContext());

                        if ( S_OK != hr )
                        {
                            JavascriptError* jsNewErrorObject = requestingScriptContext->GetLibrary()->CreateTypeError();
                            JavascriptError::SetErrorMessage(jsNewErrorObject, VBSERR_PermissionDenied, nullptr, requestingScriptContext);
                            return jsNewErrorObject;
                        }
                    }
                }
                else
                {
                    if (RecyclableObject::Is(rethrownObject))
                    {
                        if (((RecyclableObject*)rethrownObject)->GetScriptContext() != requestingScriptContext)
                        {
                            Assert(requestingScriptContext->GetHostScriptContext());
                            HRESULT hrSecurityCheck = requestingScriptContext->GetHostScriptContext()->CheckCrossDomainScriptContext(((RecyclableObject*)rethrownObject)->GetScriptContext());

                            if (hrSecurityCheck != S_OK)
                            {
                                AssertMsg(hrSecurityCheck != E_ACCESSDENIED, "Invalid cross domain throw. HRESULT must either be S_OK or !E_ACCESSDENIED.");

                                // DOM should not throw cross domain object at all. This is defend in depth that we'll return something in requestScriptContext if they do throw
                                // something bad.
                                return requestingScriptContext->GetLibrary()->GetUndefined();
                            }
                        }
                    }

                }
            }
            return rethrownObject;
        }
        return thrownObject;
    }

    FunctionBody* JavascriptExceptionObject::GetFunctionBody() const
    {
        // If it is a throwing function; it must be deserialized
        return exceptionContext.ThrowingFunction() ? exceptionContext.ThrowingFunction()->GetFunctionBody() : NULL;
    }

    JavascriptExceptionContext::StackFrame::StackFrame(JavascriptFunction* func, const JavascriptStackWalker& walker, bool initArgumentTypes)
    {
        this->functionBody = func->GetFunctionBody();

        if (this->functionBody)
        {
            this->byteCodeOffset = walker.GetByteCodeOffset();
        }
        else
        {
            this->name = walker.GetCurrentNativeLibraryEntryName();
        }

        if (this->functionBody && initArgumentTypes)
        {
            this->argumentTypes.Init(walker);
        }
    }

    bool JavascriptExceptionContext::StackFrame::IsScriptFunction() const
    {
        return functionBody != nullptr;
    }

    // Get function body -- available for script functions, null for native library builtin functions.
    FunctionBody* JavascriptExceptionContext::StackFrame::GetFunctionBody() const
    {
        return functionBody;
    }

    LPCWSTR JavascriptExceptionContext::StackFrame::GetFunctionName() const
    {
        return IsScriptFunction() ?
            GetFunctionBody()->GetExternalDisplayName() : this->name;
    }

    // Get function name with arguments info. Used by script WER.
    HRESULT JavascriptExceptionContext::StackFrame::GetFunctionNameWithArguments(_In_ LPCWSTR *outResult) const
    {
        PCWSTR name = GetFunctionName();
        HRESULT hr = S_OK;
        if (IsScriptFunction())
        {
            hr = argumentTypes.ToString(name, functionBody->GetScriptContext(), outResult);
        }
        else
        {
            *outResult = name;
        }

        return hr;
    }

    void JavascriptExceptionContext::SetThrowingFunction(JavascriptFunction * function, uint32 byteCodeOffset, void * returnAddress)
    {
        // Unfortunately, window.onerror can ask for argument.callee.caller
        // and we will return the thrown function, but the stack already unwound.
        // We will need to just box the function

        m_throwingFunction = StackScriptFunction::EnsureBoxed(BOX_PARAM(function, returnAddress, L"throw"));
        m_throwingFunctionByteCodeOffset = byteCodeOffset;
    }
#if DBG

    void JavascriptExceptionObject::FillStackBackTrace()
    {
        // Note: this->scriptContext can be NULL when we throw Out Of Memory exception.
        if (this->stackBackTrace == NULL && this->scriptContext != NULL)
        {
            Recycler* recycler = scriptContext->GetThreadContext()->GetRecycler();
            HRESULT hr = NOERROR;
            BEGIN_TRANSLATE_OOM_TO_HRESULT_NESTED
            {
                this->stackBackTrace = StackBackTrace::Capture(recycler, JavascriptExceptionObject::StackToSkip, JavascriptExceptionObject::StackTraceDepth);
            }
            END_TRANSLATE_OOM_TO_HRESULT(hr)
        }
    }
#endif
}

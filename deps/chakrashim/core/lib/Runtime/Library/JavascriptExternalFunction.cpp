//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Types\DeferredTypeHandler.h"
namespace Js
{
    // This is a wrapper class for javascript functions that are added directly to the JS engine via BuildDirectFunction
    // or CreateConstructor. We add a thunk before calling into user's direct C++ methods, with additional checks:
    // . check the script site is still alive.
    // . convert globalObject to hostObject
    // . leavescriptstart/end
    // . wrap the result value with potential cross site access

    JavascriptExternalFunction::JavascriptExternalFunction(ExternalMethod entryPoint, DynamicType* type)
        : RuntimeFunction(type, &EntryInfo::ExternalFunctionThunk), nativeMethod(entryPoint), signature(nullptr), callbackState(nullptr), initMethod(nullptr),
        oneBit(1), typeSlots(0), hasAccessors(0), callCount(0), prototypeTypeId(-1), flags(0)
    {
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptExternalFunction::JavascriptExternalFunction(ExternalMethod entryPoint, DynamicType* type, InitializeMethod method, unsigned short deferredSlotCount, bool accessors)
        : RuntimeFunction(type, &EntryInfo::ExternalFunctionThunk), nativeMethod(entryPoint), signature(nullptr), callbackState(nullptr), initMethod(method),
        oneBit(1), typeSlots(deferredSlotCount), hasAccessors(accessors), callCount(0), prototypeTypeId(-1), flags(0)
    {
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptExternalFunction::JavascriptExternalFunction(JavascriptExternalFunction* entryPoint, DynamicType* type)
        : RuntimeFunction(type, &EntryInfo::WrappedFunctionThunk), wrappedMethod(entryPoint), callbackState(nullptr), initMethod(nullptr),
        oneBit(1), typeSlots(0), hasAccessors(0), callCount(0), prototypeTypeId(-1), flags(0)
    {
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptExternalFunction::JavascriptExternalFunction(StdCallJavascriptMethod entryPoint, DynamicType* type)
        : RuntimeFunction(type, &EntryInfo::StdCallExternalFunctionThunk), stdCallNativeMethod(entryPoint), signature(nullptr), callbackState(nullptr), initMethod(nullptr),
        oneBit(1), typeSlots(0), hasAccessors(0), callCount(0), prototypeTypeId(-1), flags(0)
    {
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptExternalFunction::JavascriptExternalFunction(DynamicType *type)
        : RuntimeFunction(type, &EntryInfo::ExternalFunctionThunk), nativeMethod(nullptr), signature(nullptr), callbackState(nullptr), initMethod(nullptr),
        oneBit(1), typeSlots(0), hasAccessors(0), callCount(0), prototypeTypeId(-1), flags(0)
    {
        DebugOnly(VerifyEntryPoint());
    }

    void __cdecl JavascriptExternalFunction::DeferredInitializer(DynamicObject* instance, DeferredTypeHandlerBase* typeHandler, DeferredInitializeMode mode)
    {
        JavascriptExternalFunction* object = static_cast<JavascriptExternalFunction*>(instance);
        HRESULT hr = E_FAIL;

        ScriptContext* scriptContext = object->GetScriptContext();
        AnalysisAssert(scriptContext);
        // Don't call the implicit call if disable implicit call
        if (scriptContext->GetThreadContext()->IsDisableImplicitCall())
        {
            scriptContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_External);
            //we will return if we get call further into implicitcalls.
            return;
        }

        if (scriptContext->IsClosed() || scriptContext->IsInvalidatedForHostObjects())
        {
            Js::JavascriptError::MapAndThrowError(scriptContext, E_ACCESSDENIED);
        }
        ThreadContext* threadContext = scriptContext->GetThreadContext();

        typeHandler->Convert(instance, mode, object->typeSlots, object->hasAccessors);

        BEGIN_LEAVE_SCRIPT_INTERNAL(scriptContext)
        {
            ASYNC_HOST_OPERATION_START(threadContext);

            hr = object->initMethod(instance);

            ASYNC_HOST_OPERATION_END(threadContext);
        }
        END_LEAVE_SCRIPT_INTERNAL(scriptContext);

        if (FAILED(hr))
        {
            Js::JavascriptError::MapAndThrowError(scriptContext, hr);
        }

        JavascriptString * functionName = nullptr;
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled() &&
            object->GetFunctionName(&functionName))
        {
            object->SetPropertyWithAttributes(PropertyIds::name, functionName, PropertyConfigurable, nullptr);
        }

    }

    void JavascriptExternalFunction::PrepareExternalCall(Js::Arguments * args)
    {
        ScriptContext * scriptContext = this->type->GetScriptContext();
        Assert(!scriptContext->GetThreadContext()->IsDisableImplicitException());
        scriptContext->VerifyAlive();

        Assert(scriptContext->GetThreadContext()->IsScriptActive());

        if (args->Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined);
        }

        Var &thisVar = args->Values[0];

        Js::TypeId typeId = Js::JavascriptOperators::GetTypeId(thisVar);

        this->callCount++;
        if (IS_JS_ETW(EventEnabledJSCRIPT_HOSTING_EXTERNAL_FUNCTION_CALL_START()))
        {
            JavascriptFunction* caller = nullptr;

            // Lot the caller function if the call count of the external function pass certain threshold (randomly pick 256)
            // we don't want to call stackwalk too often. The threshold can be adjusted as needed.
            if (callCount >= ETW_MIN_COUNT_FOR_CALLER && ((callCount % ETW_MIN_COUNT_FOR_CALLER) == 0))
            {
                Js::JavascriptStackWalker stackWalker(scriptContext);
                bool foundScriptCaller = false;
                while(stackWalker.GetCaller(&caller))
                {
                    if(caller != nullptr && Js::ScriptFunction::Is(caller))
                    {
                        foundScriptCaller = true;
                        break;
                    }
                }
                if(foundScriptCaller)
                {
                    Var sourceString = caller->EnsureSourceString();
                    Assert(JavascriptString::Is(sourceString));
                    const wchar_t* callerString = Js::JavascriptString::FromVar(sourceString)->GetSz();
                    wchar_t* outString = (wchar_t*)callerString;
                    int length = 0;
                    if (wcschr(callerString, L'\n') != NULL || wcschr(callerString, L'\n') != NULL)
                    {
                        length = Js::JavascriptString::FromVar(sourceString)->GetLength();
                        outString = HeapNewArray(wchar_t, length+1);
                        int j = 0;
                        for (int i = 0; i < length; i++)
                        {
                            if (callerString[i] != L'\n' && callerString[i] != L'\r')
                            {
                                outString[j++] = callerString[i];
                            }
                        }
                        outString[j] = L'\0';
                    }
                    JS_ETW(EventWriteJSCRIPT_HOSTING_CALLER_TO_EXTERNAL(scriptContext, this, typeId, outString, callCount));
                    if (outString != callerString)
                    {
                        HeapDeleteArray(length+1, outString);
                    }
#if DBG_DUMP
                    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::HostPhase))
                    {
                        Output::Print(L"Large number of Call to trampoline: methodAddr= %p, Object typeid= %d, caller method= %s, callcount= %d\n",
                            this, typeId, callerString, callCount);
                    }
#endif
                }
            }
            JS_ETW(EventWriteJSCRIPT_HOSTING_EXTERNAL_FUNCTION_CALL_START(scriptContext, this, typeId));
#if DBG_DUMP
            if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::HostPhase))
            {
                Output::Print(L"Call to trampoline: methodAddr= %p, Object typeid= %d\n", this, typeId);
            }
#endif
        }

        Js::RecyclableObject* directHostObject = nullptr;
        switch(typeId)
        {
        case TypeIds_Integer:
#if FLOATVAR
        case TypeIds_Number:
#endif // FLOATVAR
            Assert(!Js::RecyclableObject::Is(thisVar));
            break;
        default:
            {
                Assert(Js::RecyclableObject::Is(thisVar));

                ScriptContext* scriptContextThisVar = Js::RecyclableObject::FromVar(thisVar)->GetScriptContext();
                // We need to verify "this" pointer is active as well. The problem is that DOM prototype functions are
                // the same across multiple frames, and caller can do function.call(closedthis)
                Assert(!scriptContext->GetThreadContext()->IsDisableImplicitException());
                scriptContextThisVar->VerifyAlive();

                // translate direct host for fastDOM.
                switch(typeId)
                {
                case Js::TypeIds_GlobalObject:
                    {
                        Js::GlobalObject* srcGlobalObject = static_cast<Js::GlobalObject*>(thisVar);
                        directHostObject = srcGlobalObject->GetDirectHostObject();
                        // For jsrt, direct host object can be null. If thats the case don't change it.
                        if (directHostObject != nullptr)
                        {
                            thisVar = directHostObject;
                        }

                    }
                    break;
                case Js::TypeIds_Undefined:
                case Js::TypeIds_Null:
                    {
                        // Call to DOM function with this as "undefined" or "null"
                        // This should be converted to Global object
                        Js::GlobalObject* srcGlobalObject = scriptContextThisVar->GetGlobalObject() ;
                        directHostObject = srcGlobalObject->GetDirectHostObject();
                        // For jsrt, direct host object can be null. If thats the case don't change it.
                        if (directHostObject != nullptr)
                        {
                            thisVar = directHostObject;
                        }
                    }
                    break;
                }
            }
            break;
        }
    }

    Var JavascriptExternalFunction::FinishExternalCall(Var result)
    {
        ScriptContext * scriptContext = this->type->GetScriptContext();

        if ( NULL == result )
        {
            result = scriptContext->GetLibrary()->GetUndefined();
        }
        else
        {
            result = CrossSite::MarshalVar(scriptContext, result);
        }
        if (IS_JS_ETW(EventEnabledJSCRIPT_HOSTING_EXTERNAL_FUNCTION_CALL_STOP()))
        {
            JS_ETW(EventWriteJSCRIPT_HOSTING_EXTERNAL_FUNCTION_CALL_STOP(scriptContext, this, 0));
        }
        return result;
    }

    Var JavascriptExternalFunction::ExternalFunctionThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        RUNTIME_ARGUMENTS(args, callInfo);
        JavascriptExternalFunction* externalFunction = static_cast<JavascriptExternalFunction*>(function);

        // Deferred constructors which are not callable fall back to using the RecyclableObject::DefaultEntryPoint. In order to call
        // this function we have to be inside script so all this and return before doing any of the external call preparation.
        if (externalFunction->nativeMethod == Js::RecyclableObject::DefaultExternalEntryPoint)
        {
            return Js::RecyclableObject::DefaultExternalEntryPoint(function, callInfo, args.Values);
        }

        ScriptContext * scriptContext = externalFunction->type->GetScriptContext();

#ifdef ENABLE_DIRECTCALL_TELEMETRY
        DirectCallTelemetry::AutoLogger logger(scriptContext, externalFunction, &args);
#endif

        externalFunction->PrepareExternalCall(&args);

        Var result = nullptr;
        BEGIN_LEAVE_SCRIPT_WITH_EXCEPTION(scriptContext)
        {
            // Don't do stack probe since BEGIN_LEAVE_SCRIPT_WITH_EXCEPTION does that for us already
            result = externalFunction->nativeMethod(function, callInfo, args.Values);
        }
        END_LEAVE_SCRIPT_WITH_EXCEPTION(scriptContext);

        return externalFunction->FinishExternalCall(result);
    }

    Var JavascriptExternalFunction::WrappedFunctionThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        RUNTIME_ARGUMENTS(args, callInfo);
        JavascriptExternalFunction* externalFunction = static_cast<JavascriptExternalFunction*>(function);
        ScriptContext* scriptContext = externalFunction->type->GetScriptContext();
        Assert(!scriptContext->GetThreadContext()->IsDisableImplicitException());
        scriptContext->VerifyAlive();
        Assert(scriptContext->GetThreadContext()->IsScriptActive());

        // Make sure the callee knows we are a wrapped function thunk
        args.Info.Flags = (Js::CallFlags) (((long) args.Info.Flags) | CallFlags_Wrapped);

        // don't need to leave script here, ExternalFunctionThunk will
        Assert(externalFunction->wrappedMethod->GetFunctionInfo()->GetOriginalEntryPoint() == JavascriptExternalFunction::ExternalFunctionThunk);
        return JavascriptFunction::CallFunction<true>(externalFunction->wrappedMethod, externalFunction->wrappedMethod->GetEntryPoint(), args);
    }

    Var JavascriptExternalFunction::StdCallExternalFunctionThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        RUNTIME_ARGUMENTS(args, callInfo);
        JavascriptExternalFunction* externalFunction = static_cast<JavascriptExternalFunction*>(function);

        externalFunction->PrepareExternalCall(&args);

        ScriptContext * scriptContext = externalFunction->type->GetScriptContext();
        AnalysisAssert(scriptContext);
        Var result = NULL;

        BEGIN_LEAVE_SCRIPT(scriptContext)
        {
            result = externalFunction->stdCallNativeMethod(function, ((callInfo.Flags & CallFlags_New) != 0), args.Values, args.Info.Count, externalFunction->callbackState);
        }
        END_LEAVE_SCRIPT(scriptContext);

        if (result != nullptr && !Js::TaggedNumber::Is(result))
        {
            if (!Js::RecyclableObject::Is(result))
            {
                Js::Throw::InternalError();
            }

            Js::RecyclableObject * obj = Js::RecyclableObject::FromVar(result);

            // For JSRT, we could get result marshalled in different context.
            bool isJSRT = !(scriptContext->GetThreadContext()->GetIsThreadBound());
            if (!isJSRT && obj->GetScriptContext() != scriptContext)
            {
                Js::Throw::InternalError();
            }
        }


        if (scriptContext->HasRecordedException())
        {
            bool considerPassingToDebugger = false;
            JavascriptExceptionObject* recordedException = scriptContext->GetAndClearRecordedException(&considerPassingToDebugger);
            if (recordedException != nullptr)
            {
                // If this is script termination, then throw ScriptAbortExceptio, else throw normal Exception object.
                if (recordedException == scriptContext->GetThreadContext()->GetPendingTerminatedErrorObject())
                {
                    throw Js::ScriptAbortException();
                }
                else
                {
                    JavascriptExceptionOperators::RethrowExceptionObject(recordedException, scriptContext, considerPassingToDebugger);
                }
            }
        }

        return externalFunction->FinishExternalCall(result);
    }

    BOOL JavascriptExternalFunction::SetLengthProperty(Var length)
    {
        return DynamicObject::SetPropertyWithAttributes(PropertyIds::length, length, PropertyConfigurable, NULL, PropertyOperation_None, SideEffects_None);
    }

}

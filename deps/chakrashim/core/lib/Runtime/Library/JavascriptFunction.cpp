//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "BackEndAPI.h"
#include "Library\StackScriptFunction.h"
#include "Types\SpreadArgument.h"

#ifdef _M_X64
#include "ByteCode\PropertyIdArray.h"
#include "Language\AsmJsTypes.h"
#include "Language\AsmJsModule.h"
#endif

extern "C" PVOID _ReturnAddress(VOID);
#pragma intrinsic(_ReturnAddress)

#ifdef _M_IX86
#ifdef _CONTROL_FLOW_GUARD
extern "C" PVOID __guard_check_icall_fptr;
#endif
extern "C" void __cdecl _alloca_probe_16();
#endif

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(JavascriptFunction);
    JavascriptFunction::JavascriptFunction(DynamicType * type)
        : DynamicObject(type), functionInfo(nullptr), constructorCache(&ConstructorCache::DefaultInstance)
    {
        Assert(this->constructorCache != nullptr);
    }


    JavascriptFunction::JavascriptFunction(DynamicType * type, FunctionInfo * functionInfo)
        : DynamicObject(type), functionInfo(functionInfo), constructorCache(&ConstructorCache::DefaultInstance)

    {
        Assert(this->constructorCache != nullptr);
        this->GetTypeHandler()->ClearHasOnlyWritableDataProperties(); // length is non-writable
        if (GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsPrototypeFlag)
        {
            // No need to invalidate store field caches for non-writable properties here. Since this type is just being created, it cannot represent
            // an object that is already a prototype. If it becomes a prototype and then we attempt to add a property to an object derived from this
            // object, then we will check if this property is writable, and only if it is will we do the fast path for add property.
            // GetScriptContext()->InvalidateStoreFieldCaches(PropertyIds::length);
            GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }
    }

    JavascriptFunction::JavascriptFunction(DynamicType * type, FunctionInfo * functionInfo, ConstructorCache* cache)
        : DynamicObject(type), functionInfo(functionInfo), constructorCache(cache)

    {
        Assert(this->constructorCache != nullptr);
        this->GetTypeHandler()->ClearHasOnlyWritableDataProperties(); // length is non-writable
        if (GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsPrototypeFlag)
        {
            // No need to invalidate store field caches for non-writable properties here. Since this type is just being created, it cannot represent
            // an object that is already a prototype. If it becomes a prototype and then we attempt to add a property to an object derived from this
            // object, then we will check if this property is writable, and only if it is will we do the fast path for add property.
            // GetScriptContext()->InvalidateStoreFieldCaches(PropertyIds::length);
            GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }
    }

    FunctionProxy *JavascriptFunction::GetFunctionProxy() const
    {
        Assert(functionInfo != nullptr);
        return functionInfo->GetFunctionProxy();
    }

    ParseableFunctionInfo *JavascriptFunction::GetParseableFunctionInfo() const
    {
        Assert(functionInfo != nullptr);
        return functionInfo->GetParseableFunctionInfo();
    }

    DeferDeserializeFunctionInfo *JavascriptFunction::GetDeferDeserializeFunctionInfo() const
    {
        Assert(functionInfo != nullptr);
        return functionInfo->GetDeferDeserializeFunctionInfo();
    }

    FunctionBody *JavascriptFunction::GetFunctionBody() const
    {
        Assert(functionInfo != nullptr);
        return functionInfo->GetFunctionBody();
    }

    BOOL JavascriptFunction::IsScriptFunction() const
    {
        Assert(functionInfo != nullptr);
        return functionInfo->HasBody();
    }

    bool JavascriptFunction::Is(Var aValue)
    {
        if (JavascriptOperators::GetTypeId(aValue) == TypeIds_Function)
        {
            return true;
        }
        return false;
    }

    JavascriptFunction* JavascriptFunction::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptFunction'");

        return static_cast<JavascriptFunction *>(RecyclableObject::FromVar(aValue));
    }

    BOOL JavascriptFunction::IsStrictMode() const
    {
        FunctionProxy * proxy = this->GetFunctionProxy();
        return proxy && proxy->EnsureDeserialized()->GetIsStrictMode();
    }

    BOOL JavascriptFunction::IsLambda() const
    {
        return this->GetFunctionInfo()->IsLambda();
    }

    BOOL JavascriptFunction::IsConstructor() const
    {
        return this->GetFunctionInfo()->IsConstructor();
    }

#if DBG
    /* static */
    bool JavascriptFunction::IsBuiltinProperty(Var objectWithProperty, PropertyIds propertyId)
    {
        return ScriptFunction::Is(objectWithProperty)
            && (propertyId == PropertyIds::length || (JavascriptFunction::FromVar(objectWithProperty)->HasRestrictedProperties() && (propertyId == PropertyIds::arguments || propertyId == PropertyIds::caller)));
    }
#endif

    static wchar_t const funcName[] = L"function anonymous";
    static wchar_t const genFuncName[] = L"function* anonymous";
    static wchar_t const bracket[] = L" {\012";

    Var JavascriptFunction::NewInstanceHelper(ScriptContext *scriptContext, RecyclableObject* function, CallInfo callInfo, Js::ArgumentReader& args, bool isGenerator /* = false */)
    {
        JavascriptLibrary* library = function->GetLibrary();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        // SkipDefaultNewObject function flag should have prevented the default object from
        // being created, except when call true a host dispatch.
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && RecyclableObject::Is(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr
            || JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch);

        JavascriptString* separator = library->GetCommaDisplayString();

        // Gather all the formals into a string like (fml1, fml2, fml3)
        JavascriptString *formals = library->CreateStringFromCppLiteral(L"(");
        for (uint i = 1; i < args.Info.Count - 1; ++i)
        {
            if (i != 1)
            {
                formals = JavascriptString::Concat(formals, separator);
            }
            formals = JavascriptString::Concat(formals, JavascriptConversion::ToString(args.Values[i], scriptContext));
        }
        formals = JavascriptString::Concat(formals, library->CreateStringFromCppLiteral(L")"));

        // Function body, last argument to Function(...)
        JavascriptString *fnBody = NULL;
        if (args.Info.Count > 1)
        {
            fnBody = JavascriptConversion::ToString(args.Values[args.Info.Count - 1], scriptContext);
        }

        // Create a string representing the anonymous function
        Assert(CountNewlines(funcName) + CountNewlines(bracket) == numberLinesPrependedToAnonymousFunction); // Be sure to add exactly one line to anonymous function

        JavascriptString *bs = isGenerator ?
            library->CreateStringFromCppLiteral(genFuncName) :
            library->CreateStringFromCppLiteral(funcName);
        bs = JavascriptString::Concat(bs, formals);
        bs = JavascriptString::Concat(bs, library->CreateStringFromCppLiteral(bracket));
        if (fnBody != NULL)
        {
            bs = JavascriptString::Concat(bs, fnBody);
        }

        bs = JavascriptString::Concat(bs, library->CreateStringFromCppLiteral(L"\012}"));

        // Bug 1105479. Get the module id from the caller
        ModuleID moduleID = kmodGlobal;

        BOOL strictMode = FALSE;

        JavascriptFunction *pfuncScript;
        ParseableFunctionInfo *pfuncBodyCache = NULL;
        wchar_t const * sourceString = bs->GetSz();
        charcount_t sourceLen = bs->GetLength();
        EvalMapString key(sourceString, sourceLen, moduleID, strictMode, /* isLibraryCode = */ false);
        if (!scriptContext->IsInNewFunctionMap(key, &pfuncBodyCache))
        {
            // ES3 and ES5 specs require validation of the formal list and the function body

            // Validate formals here
            scriptContext->GetGlobalObject()->ValidateSyntax(scriptContext, formals->GetSz(), formals->GetLength(), isGenerator, &Parser::ValidateFormals);
            if (fnBody != NULL)
            {
                // Validate function body
                scriptContext->GetGlobalObject()->ValidateSyntax(scriptContext, fnBody->GetSz(), fnBody->GetLength(), isGenerator, &Parser::ValidateSourceElementList);
            }

            pfuncScript = scriptContext->GetGlobalObject()->EvalHelper(scriptContext, sourceString, sourceLen, moduleID, fscrNil, Constants::FunctionCode, TRUE, TRUE, strictMode);

            // Indicate that this is a top-level function. We don't pass the fscrGlobalCode flag to the eval helper,
            // or it will return the global function that wraps the declared function body, as though it were an eval.
            // But we want, for instance, to be able to verify that we did the right amount of deferred parsing.
            ParseableFunctionInfo *functionInfo = pfuncScript->GetParseableFunctionInfo();
            Assert(functionInfo);
            functionInfo->SetGrfscr(functionInfo->GetGrfscr() | fscrGlobalCode);

            scriptContext->AddToNewFunctionMap(key, functionInfo);
        }
        else
        {
            // Get the latest proxy
            FunctionProxy * proxy = pfuncBodyCache->GetFunctionProxy();
            if (proxy->IsGenerator())
            {
                pfuncScript = scriptContext->GetLibrary()->CreateGeneratorVirtualScriptFunction(proxy);
            }
            else
            {
                pfuncScript = scriptContext->GetLibrary()->CreateScriptFunction(proxy);
            }
        }

        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_FUNCTION(pfuncScript, EtwTrace::GetFunctionId(pfuncScript->GetFunctionProxy())));

        if (isGenerator)
        {
            Assert(pfuncScript->GetFunctionInfo()->IsGenerator());
            auto pfuncVirt = static_cast<GeneratorVirtualScriptFunction*>(pfuncScript);
            auto pfuncGen = scriptContext->GetLibrary()->CreateGeneratorFunction(JavascriptGeneratorFunction::EntryGeneratorFunctionImplementation, pfuncVirt);
            pfuncVirt->SetRealGeneratorFunction(pfuncGen);
            pfuncScript = pfuncGen;
        }

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), pfuncScript, nullptr, scriptContext) :
            pfuncScript;
    }

    Var JavascriptFunction::NewInstanceRestrictedMode(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ScriptContext* scriptContext = function->GetScriptContext();

        scriptContext->CheckEvalRestriction();

        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);

        return NewInstanceHelper(scriptContext, function, callInfo, args);
    }

    Var JavascriptFunction::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        return NewInstanceHelper(scriptContext, function, callInfo, args);
    }

    //
    // Dummy EntryPoint for Function.prototype
    //
    Var JavascriptFunction::PrototypeEntryPoint(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = function->GetLibrary();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        if (callInfo.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, VBSERR_ActionNotSupported);
        }

        return library->GetUndefined();
    }

    BOOL JavascriptFunction::IsThrowTypeErrorFunction()
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        Assert(scriptContext);

        return
            this == scriptContext->GetLibrary()->GetThrowTypeErrorAccessorFunction() ||
            this == scriptContext->GetLibrary()->GetThrowTypeErrorCalleeAccessorFunction() ||
            this == scriptContext->GetLibrary()->GetThrowTypeErrorCallerAccessorFunction() ||
            this == scriptContext->GetLibrary()->GetThrowTypeErrorArgumentsAccessorFunction();
    }

    enum : unsigned { STACK_ARGS_ALLOCA_THRESHOLD = 8 }; // Number of stack args we allow before using _alloca

    // ES5 15.3.4.3
    //When the apply method is called on an object func with arguments thisArg and argArray the following steps are taken:
    //    1.    If IsCallable(func) is false, then throw a TypeError exception.
    //    2.    If argArray is null or undefined, then
    //          a.      Return the result of calling the [[Call]] internal method of func, providing thisArg as the this value and an empty list of arguments.
    //    3.    If Type(argArray) is not Object, then throw a TypeError exception.
    //    4.    Let len be the result of calling the [[Get]] internal method of argArray with argument "length".
    //
    //    Steps 5 and 7 deleted from July 19 Errata of ES5 spec
    //
    //    5.    If len is null or undefined, then throw a TypeError exception.
    //    6.    Len n be ToUint32(len).
    //    7.    If n is not equal to ToNumber(len), then throw a TypeError exception.
    //    8.    Let argList  be an empty List.
    //    9.    Let index be 0.
    //    10.   Repeat while index < n
    //          a.      Let indexName be ToString(index).
    //          b.      Let nextArg be the result of calling the [[Get]] internal method of argArray with indexName as the argument.
    //          c.      Append nextArg as the last element of argList.
    //          d.      Set index to index + 1.
    //    11.   Return the result of calling the [[Call]] internal method of func, providing thisArg as the this value and argList as the list of arguments.
    //    The length property of the apply method is 2.

    Var JavascriptFunction::EntryApply(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        // Ideally, we want to maintain CallFlags_Eval behavior and pass along the extra FrameDisplay parameter
        // but that we would be a bigger change than what we want to do in this ship cycle. See WIN8: 915315.
        // If eval is executed using apply it will not get the frame display and always execute in global scope.
        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        ///
        /// Check Argument[0] has internal [[Call]] property
        /// If not, throw TypeError
        ///
        if (args.Info.Count == 0 || !JavascriptConversion::IsCallable(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedFunction, L"Function.prototype.apply");
        }

        Var thisVar = NULL;
        Var argArray = NULL;
        RecyclableObject* pFunc = RecyclableObject::FromVar(args[0]);

        if (args.Info.Count == 1)
        {
            thisVar = scriptContext->GetLibrary()->GetUndefined();
        }
        else if (args.Info.Count == 2)
        {
            thisVar = args.Values[1];
        }
        else if (args.Info.Count > 2)
        {
            thisVar = args.Values[1];
            argArray = args.Values[2];
        }

        return CalloutHelper<false>(pFunc, thisVar, /* overridingNewTarget = */nullptr, argArray, scriptContext);
    }

    template <bool isConstruct>
    Var JavascriptFunction::CalloutHelper(RecyclableObject* pFunc, Var thisVar, Var overridingNewTarget, Var argArray, ScriptContext* scriptContext)
    {
        CallFlags callFlag;
        if (isConstruct)
        {
            callFlag = CallFlags_New;
        }
        else
        {
            callFlag = CallFlags_Value;
        }
        Arguments outArgs(CallInfo(callFlag, 0), nullptr);

        Var stackArgs[STACK_ARGS_ALLOCA_THRESHOLD];

        if (nullptr == argArray)
        {
            outArgs.Info.Count = 1;
            outArgs.Values = &thisVar;
        }
        else
        {
            bool isArray = JavascriptArray::Is(argArray);
            TypeId typeId = JavascriptOperators::GetTypeId(argArray);
            bool isNullOrUndefined = (typeId == TypeIds_Null || typeId == TypeIds_Undefined);

            if (!isNullOrUndefined && !JavascriptOperators::IsObject(argArray)) // ES5: throw if Type(argArray) is not Object
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Function.prototype.apply");
            }

            int64 len;
            JavascriptArray* arr = NULL;
            RecyclableObject* dynamicObject = RecyclableObject::FromVar(argArray);

            if (isNullOrUndefined)
            {
                len = 0;
            }
            else if (isArray)
            {
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(argArray);
#endif
                arr = JavascriptArray::FromVar(argArray);
                len = arr->GetLength();
            }
            else
            {
                Var lenProp = JavascriptOperators::OP_GetLength(dynamicObject, scriptContext);
                len = JavascriptConversion::ToLength(lenProp, scriptContext);
            }

            if (len >= CallInfo::kMaxCountArgs)
            {
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgListTooLarge);
            }

            outArgs.Info.Count = (uint)len + 1;
            if (len == 0)
            {
                outArgs.Values = &thisVar;
            }
            else
            {
                if (outArgs.Info.Count > STACK_ARGS_ALLOCA_THRESHOLD)
                {
                    PROBE_STACK(scriptContext, outArgs.Info.Count * sizeof(Var)+Js::Constants::MinStackDefault); // args + function call
                    outArgs.Values = (Var*)_alloca(outArgs.Info.Count * sizeof(Var));
                }
                else
                {
                    outArgs.Values = stackArgs;
                }
                outArgs.Values[0] = thisVar;


                Var undefined = pFunc->GetLibrary()->GetUndefined();
                if (isArray && arr->GetScriptContext() == scriptContext)
                {
                    arr->ForEachItemInRange<false>(0, (uint)len, undefined, scriptContext,
                        [&outArgs](uint index, Var element)
                    {
                        outArgs.Values[index + 1] = element;
                    });
                }
                else
                {
                    for (uint i = 0; i < len; i++)
                    {
                        Var element;
                        if (!JavascriptOperators::GetItem(dynamicObject, i, &element, scriptContext))
                        {
                            element = undefined;
                        }
                        outArgs.Values[i + 1] = element;
                    }
                }
            }
        }

        if (isConstruct)
        {
            return JavascriptFunction::CallAsConstructor(pFunc, overridingNewTarget, outArgs, scriptContext);
        }
        else
        {
            return JavascriptFunction::CallFunction<true>(pFunc, pFunc->GetEntryPoint(), outArgs);
        }
    }

    Var JavascriptFunction::ApplyHelper(RecyclableObject* function, Var thisArg, Var argArray, ScriptContext* scriptContext)
    {
        return CalloutHelper<false>(function, thisArg, /* overridingNewTarget = */nullptr, argArray, scriptContext);
    }

    Var JavascriptFunction::ConstructHelper(RecyclableObject* function, Var thisArg, Var overridingNewTarget, Var argArray, ScriptContext* scriptContext)
    {
        return CalloutHelper<true>(function, thisArg, overridingNewTarget, argArray, scriptContext);
    }

    Var JavascriptFunction::EntryBind(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(FunctionBindCount);

        Assert(!(callInfo.Flags & CallFlags_New));

        ///
        /// Check Argument[0] has internal [[Call]] property
        /// If not, throw TypeError
        ///
        if (args.Info.Count == 0 || !JavascriptConversion::IsCallable(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedFunction, L"Function.prototype.bind");
        }

        BoundFunction* boundFunc = BoundFunction::New(scriptContext, args);

        return boundFunc;
    }

    // ES5 15.3.4.4
    // Function.prototype.call (thisArg [ , arg1 [ , arg2, ... ] ] )
    //    When the call method is called on an object func with argument thisArg and optional arguments arg1, arg2 etc, the following steps are taken:
    //    1.    If IsCallable(func) is false, then throw a TypeError exception.
    //    2.    Let argList be an empty List.
    //    3.    If this method was called with more than one argument then in left to right order starting with arg1 append each argument as the last element of argList
    //    4.    Return the result of calling the [[Call]] internal method of func, providing thisArg as the this value and argList as the list of arguments.
    //    The length property of the call method is 1.

    Var JavascriptFunction::EntryCall(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        RUNTIME_ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        ///
        /// Check Argument[0] has internal [[Call]] property
        /// If not, throw TypeError
        ///
        if (args.Info.Count == 0 || !JavascriptConversion::IsCallable(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedFunction, L"Function.prototype.call");
        }

        RecyclableObject *pFunc = RecyclableObject::FromVar(args[0]);
        if (args.Info.Count == 1)
        {
            args.Values[0] = scriptContext->GetLibrary()->GetUndefined();
        }
        else
        {
            ///
            /// Remove function object from the arguments and pass the rest
            ///
            for (uint i = 0; i < args.Info.Count - 1; ++i)
            {
                args.Values[i] = args.Values[i + 1];
            }
            args.Info.Count = args.Info.Count - 1;
        }

        ///
        /// Call the [[Call]] method on the function object
        ///
        return JavascriptFunction::CallFunction<true>(pFunc, pFunc->GetEntryPoint(), args);
    }

    Var JavascriptFunction::CallRootFunctionInScript(JavascriptFunction* func, Arguments args)
    {
        ScriptContext* scriptContext = func->GetScriptContext();
        if (scriptContext->GetThreadContext()->HasPreviousHostScriptContext())
        {
            ScriptContext* requestContext = scriptContext->GetThreadContext()->GetPreviousHostScriptContext()->GetScriptContext();
            func = JavascriptFunction::FromVar(CrossSite::MarshalVar(requestContext, func));
        }
        return func->CallRootFunction(args, scriptContext, true);
    }
    Var JavascriptFunction::CallRootFunction(Arguments args, ScriptContext * scriptContext, bool inScript)
    {

#ifdef _M_X64
        Var ret = nullptr;

#ifdef FAULT_INJECTION
        if (Js::Configuration::Global.flags.FaultInjection >= 0)
        {
            Js::FaultInjection::pfnHandleAV = JavascriptFunction::ResumeForOutOfBoundsArrayRefs;
            __try
            {
                ret = CallRootFunctionInternal(args, scriptContext, inScript);
            }
            __finally
            {
                Js::FaultInjection::pfnHandleAV = nullptr;
            }
            //ret should never be null here
            Assert(ret);
            return ret;
        }
#endif

        __try
        {
            ret = CallRootFunctionInternal(args, scriptContext, inScript);
        }
        __except (ResumeForOutOfBoundsArrayRefs(GetExceptionCode(), GetExceptionInformation()))
        {
            // should never reach here
            Assert(false);
        }
        //ret should never be null here
        Assert(ret);
        return ret;
#else
        return CallRootFunctionInternal(args, scriptContext, inScript);
#endif
    }
    Var JavascriptFunction::CallRootFunctionInternal(Arguments args, ScriptContext * scriptContext, bool inScript)
    {
#if DBG
        if (IsInAssert != 0)
        {
            // Just don't execute anything if we are in an assert
            // throw the exception directly to avoid additional assert in Js::Throw::InternalError
            throw Js::InternalErrorException();
        }
#endif

        if (inScript)
        {
            Assert(!(args.Info.Flags & CallFlags_New));
            return JavascriptFunction::CallFunction<true>(this, GetEntryPoint(), args);
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        Js::Var varThis;
        if (PHASE_FORCE1(Js::EvalCompilePhase) && args.Info.Count == 0)
        {
            varThis = JavascriptOperators::OP_GetThis(scriptContext->GetLibrary()->GetUndefined(), kmodGlobal, scriptContext);
            args.Info.Flags = (Js::CallFlags)(args.Info.Flags | CallFlags_Eval);
            args.Info.Count = 1;
            args.Values = &varThis;
        }
#endif

        Var varResult = nullptr;
        ThreadContext *threadContext;
        threadContext = scriptContext->GetThreadContext();

        JavascriptExceptionObject* pExceptionObject = NULL;
        bool hasCaller = scriptContext->GetHostScriptContext() ? !!scriptContext->GetHostScriptContext()->HasCaller() : false;
        Assert(scriptContext == GetScriptContext());
        BEGIN_JS_RUNTIME_CALLROOT_EX(scriptContext, hasCaller)
        {
            scriptContext->VerifyAlive(true);
            try
            {
                varResult =
                    args.Info.Flags & CallFlags_New ?
                    CallAsConstructor(this, /* overridingNewTarget = */nullptr, args, scriptContext) :
                    CallFunction<true>(this, this->GetEntryPoint(), args);

                // A recent compiler bug 150148 can incorrectly eliminate catch block, temporary workaround
                if (threadContext == NULL)
                {
                    throw (JavascriptExceptionObject*)NULL;
                }
            }
            catch (JavascriptExceptionObject* exceptionObject)
            {
                pExceptionObject = exceptionObject;
            }

            if (pExceptionObject)
            {
                pExceptionObject = pExceptionObject->CloneIfStaticExceptionObject(scriptContext);
                throw pExceptionObject;
            }
        }
        END_JS_RUNTIME_CALL(scriptContext);

        Assert(varResult != nullptr);
        return varResult;
    }

#if DBG
    /*static*/
    void JavascriptFunction::CheckValidDebugThunk(ScriptContext* scriptContext, RecyclableObject *function)
    {
        Assert(scriptContext != nullptr);
        Assert(function != nullptr);

        if (scriptContext->IsInDebugMode()
            && !scriptContext->IsInterpreted() && !CONFIG_FLAG(ForceDiagnosticsMode)    // Does not work nicely if we change the default settings.
            && function->GetEntryPoint() != scriptContext->CurrentThunk
            && function->GetEntryPoint() != scriptContext->CurrentCrossSiteThunk
            && JavascriptFunction::Is(function))
        {

            JavascriptFunction *jsFunction = JavascriptFunction::FromVar(function);
            if (!jsFunction->IsBoundFunction()
                && !jsFunction->GetFunctionInfo()->IsDeferred()
                && (jsFunction->GetFunctionInfo()->GetAttributes() & FunctionInfo::DoNotProfile) != FunctionInfo::DoNotProfile
                && jsFunction->GetFunctionInfo() != &JavascriptExternalFunction::EntryInfo::WrappedFunctionThunk)
            {
                Js::FunctionProxy *proxy = jsFunction->GetFunctionProxy();
                if (proxy)
                {
                    AssertMsg(proxy->HasValidEntryPoint(), "Function does not have valid entrypoint");
                }
            }
        }
    }
#endif

    Var JavascriptFunction::CallAsConstructor(Var v, Var overridingNewTarget, Arguments args, ScriptContext* scriptContext, const Js::AuxArray<uint32> *spreadIndices)
    {
        Assert(v);
        Assert(args.Info.Flags & CallFlags_New);
        Assert(scriptContext);

        // newCount is ushort.
        if (args.Info.Count >= USHORT_MAX)
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgListTooLarge);
        }
        AnalysisAssert(args.Info.Count < USHORT_MAX);

        // Create the empty object if necessary:
        // - Built-in constructor functions will return a new object of a specific type, so a new empty object does not need to
        //   be created
        // - If the newTarget is specified and the function is base kind then the this object will be already created. So we can
        //   just use it instead of creating a new one.
        // - For user-defined constructor functions, an empty object is created with the function's prototype
        Var resultObject = nullptr;
        if (overridingNewTarget != nullptr && args.Info.Count > 0)
        {
            resultObject = args.Values[0];
        }
        else
        {
            resultObject = JavascriptOperators::NewScObjectNoCtor(v, scriptContext);
        }

        // JavascriptOperators::NewScObject should have thrown if 'v' is not a constructor
        RecyclableObject* functionObj = RecyclableObject::FromVar(v);

        Var* newValues = args.Values;
        CallFlags newFlags = args.Info.Flags;

        ushort newCount = args.Info.Count;
        bool thisAlreadySpecified = false;

        if (overridingNewTarget != nullptr)
        {
            if (ScriptFunction::Is(functionObj) && ScriptFunction::FromVar(functionObj)->GetFunctionInfo()->IsClassConstructor())
            {
                thisAlreadySpecified = true;
                args.Values[0] = overridingNewTarget;
            }
            else
            {
                newCount++;
                newFlags = (CallFlags)(newFlags | CallFlags_NewTarget | CallFlags_ExtraArg);
                const unsigned STACK_ARGS_ALLOCA_THRESHOLD = 8; // Number of stack args we allow before using _alloca
                Var stackArgs[STACK_ARGS_ALLOCA_THRESHOLD];
                if (newCount > STACK_ARGS_ALLOCA_THRESHOLD)
                {
                    PROBE_STACK(scriptContext, newCount * sizeof(Var) + Js::Constants::MinStackDefault); // args + function call
                    newValues = (Var*)_alloca(newCount * sizeof(Var));
                }
                else
                {
                    newValues = stackArgs;
                }

                for (unsigned int i = 0; i < args.Info.Count; i++)
                {
                    newValues[i] = args.Values[i];
                }
#pragma prefast(suppress:6386, "The index is within the bounds")
                newValues[args.Info.Count] = overridingNewTarget;
            }
        }

        // Call the constructor function:
        // - If this is not already specified as the overriding new target in Reflect.construct a class case, then
        // - Pass in the new empty object as the 'this' parameter. This can be null if an empty object was not created.

        if (!thisAlreadySpecified)
        {
            newValues[0] = resultObject;
        }

        CallInfo newCallInfo(newFlags, newCount);
        Arguments newArgs(newCallInfo, newValues);

        if (JavascriptProxy::Is(v))
        {
            JavascriptProxy* proxy = JavascriptProxy::FromVar(v);
            return proxy->ConstructorTrap(newArgs, scriptContext, spreadIndices);
        }

#if DBG
        if (scriptContext->IsInDebugMode())
        {
            CheckValidDebugThunk(scriptContext, functionObj);
        }
#endif

        Var functionResult;
        if (spreadIndices != nullptr)
        {
            functionResult = CallSpreadFunction(functionObj, functionObj->GetEntryPoint(), newArgs, spreadIndices);
        }
        else
        {
            functionResult = CallFunction<true>(functionObj, functionObj->GetEntryPoint(), newArgs);
        }

        return
            FinishConstructor(
                functionResult,
                resultObject,
                JavascriptFunction::Is(functionObj) && functionObj->GetScriptContext() == scriptContext ?
                JavascriptFunction::FromVar(functionObj) :
                nullptr);
    }

    Var JavascriptFunction::FinishConstructor(
        const Var constructorReturnValue,
        Var newObject,
        JavascriptFunction *const function)
    {
        Assert(constructorReturnValue);

        // CONSIDER: Using constructorCache->ctorHasNoExplicitReturnValue to speed up this interpreter code path.
        if (JavascriptOperators::IsObject(constructorReturnValue))
        {
            newObject = constructorReturnValue;
        }

        if (function && function->GetConstructorCache()->NeedsUpdateAfterCtor())
        {
            JavascriptOperators::UpdateNewScObjectCache(function, newObject, function->GetScriptContext());
        }

        return newObject;
    }

    Var JavascriptFunction::EntrySpreadCall(const Js::AuxArray<uint32> *spreadIndices, RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        RUNTIME_ARGUMENTS(args, callInfo);

        return JavascriptFunction::CallSpreadFunction(function, function->GetEntryPoint(), args, spreadIndices);
    }

    uint32 JavascriptFunction::GetSpreadSize(const Arguments args, const Js::AuxArray<uint32> *spreadIndices, ScriptContext *scriptContext)
    {
        // Work out the expanded number of arguments.
        uint32 totalLength = args.Info.Count - spreadIndices->count;
        ::Math::RecordOverflowPolicy overflow;
        for (unsigned i = 0; i < spreadIndices->count; ++i)
        {
            uint32 elementLength = JavascriptArray::GetSpreadArgLen(args[spreadIndices->elements[i]], scriptContext);
            totalLength = UInt32Math::Add(totalLength, elementLength, overflow);
        }

        if (totalLength >= CallInfo::kMaxCountArgs || overflow.HasOverflowed())
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgListTooLarge);
        }

        return totalLength;
    }

    void JavascriptFunction::SpreadArgs(const Arguments args, Arguments& destArgs, const Js::AuxArray<uint32> *spreadIndices, ScriptContext *scriptContext)
    {
        Assert(args.Values != nullptr);
        Assert(destArgs.Values != nullptr);

        CallInfo callInfo = args.Info;
        size_t destArgsByteSize = destArgs.Info.Count * sizeof(Var);

        destArgs.Values[0] = args[0];

        // Iterate over the arguments, spreading inline. We skip 'this'.
        Var undefined = scriptContext->GetLibrary()->GetUndefined();

        for (unsigned i = 1, argsIndex = 1, spreadArgIndex = 0; i < callInfo.Count; ++i)
        {
            uint32 spreadIndex = spreadIndices->elements[spreadArgIndex]; // Next index to be spread.
            if (i < spreadIndex)
            {
                // Copy everything until the next spread index.
                js_memcpy_s(destArgs.Values + argsIndex,
                            destArgsByteSize - (argsIndex * sizeof(Var)),
                            args.Values + i,
                            (spreadIndex - i) * sizeof(Var));
                argsIndex += spreadIndex - i;
                i = spreadIndex - 1;
                continue;
            }
            else if (i > spreadIndex)
            {
                // Copy everything after the last spread index.
                js_memcpy_s(destArgs.Values + argsIndex,
                            destArgsByteSize - (argsIndex * sizeof(Var)),
                            args.Values + i,
                            (args.Info.Count - i) * sizeof(Var));
                break;
            }
            else
            {
                // Expand the spread element.
                Var instance = args[spreadIndex];

                if (SpreadArgument::Is(instance))
                {
                    SpreadArgument* spreadedArgs = SpreadArgument::FromVar(instance);
                    uint size = spreadedArgs->GetArgumentSpreadCount();
                    const Var * spreadBuffer = spreadedArgs->GetArgumentSpread();
                    js_memcpy_s(destArgs.Values + argsIndex,
                        size * sizeof(Var),
                        spreadBuffer,
                        size * sizeof(Var));
                    argsIndex += size;
                }
                else
                {
                    AssertMsg(JavascriptArray::Is(instance) || TypedArrayBase::Is(instance), "Only SpreadArgument, TypedArray, and JavascriptArray should be listed as spread arguments");

                    // We first try to interpret the spread parameter as a JavascriptArray.
                    JavascriptArray *arr = nullptr;
                    if (JavascriptArray::Is(instance))
                    {
                        arr = JavascriptArray::FromVar(instance);
                    }

                    if (arr != nullptr && !arr->IsCrossSiteObject())
                    {
                        // CONSIDER: Optimize by creating a JavascriptArray routine which allows
                        // memcpy-like semantics in optimal situations (no gaps, etc.)
                        if (argsIndex + arr->GetLength() > destArgs.Info.Count)
                        {
                            AssertMsg(false, "The array length has changed since we allocated the destArgs buffer?");
                            Throw::FatalInternalError();
                        }

                        for (uint32 j = 0; j < arr->GetLength(); j++)
                        {
                            Var element;
                            if (!arr->DirectGetItemAtFull(j, &element))
                            {
                                element = undefined;
                            }
                            destArgs.Values[argsIndex++] = element;
                        }
                    }
                    else
                    {
                        // Emulate %ArrayPrototype%.values() iterator; basically iterate from 0 to length
                        RecyclableObject *propertyObject;
                        if (!JavascriptOperators::GetPropertyObject(instance, scriptContext, &propertyObject))
                        {
                            JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidSpreadArgument);
                        }

                        uint32 len = JavascriptArray::GetSpreadArgLen(instance, scriptContext);
                        if (argsIndex + len > destArgs.Info.Count)
                        {
                            AssertMsg(false, "The array length has changed since we allocated the destArgs buffer?");
                            Throw::FatalInternalError();
                        }

                        for (uint j = 0; j < len; j++)
                        {
                            Var element;
                            if (!JavascriptOperators::GetItem(instance, propertyObject, j, &element, scriptContext))
                            {
                                element = undefined;
                            }
                            destArgs.Values[argsIndex++] = element;
                        }
                    }
                }

                if (spreadArgIndex < spreadIndices->count - 1)
                {
                    spreadArgIndex++;
                }
            }
        }
    }

    Var JavascriptFunction::CallSpreadFunction(RecyclableObject* function, JavascriptMethod entryPoint, Arguments args, const Js::AuxArray<uint32> *spreadIndices)
    {
        ScriptContext* scriptContext = function->GetScriptContext();

        // Work out the expanded number of arguments.
        uint32 actualLength = GetSpreadSize(args, spreadIndices, scriptContext);

        // Allocate (if needed) space for the expanded arguments.
        Arguments outArgs(CallInfo(args.Info.Flags, 0), nullptr);
        outArgs.Info.Count = actualLength;
        Var stackArgs[STACK_ARGS_ALLOCA_THRESHOLD];
        size_t outArgsSize = 0;
        if (outArgs.Info.Count > STACK_ARGS_ALLOCA_THRESHOLD)
        {
            PROBE_STACK(scriptContext, outArgs.Info.Count * sizeof(Var) + Js::Constants::MinStackDefault); // args + function call
            outArgsSize = outArgs.Info.Count * sizeof(Var);
            outArgs.Values = (Var*)_alloca(outArgsSize);
        }
        else
        {
            outArgs.Values = stackArgs;
            outArgsSize = STACK_ARGS_ALLOCA_THRESHOLD * sizeof(Var);
            ZeroMemory(outArgs.Values, outArgsSize); // We may not use all of the elements
        }

        SpreadArgs(args, outArgs, spreadIndices, scriptContext);

        return JavascriptFunction::CallFunction<true>(function, entryPoint, outArgs);
    }

    Var JavascriptFunction::CallFunction(Arguments args)
    {
        return JavascriptFunction::CallFunction<true>(this, this->GetEntryPoint(), args);
    }

    template Var JavascriptFunction::CallFunction<true>(RecyclableObject* function, JavascriptMethod entryPoint, Arguments args);
    template Var JavascriptFunction::CallFunction<false>(RecyclableObject* function, JavascriptMethod entryPoint, Arguments args);

#ifdef _M_IX86
    template <bool doStackProbe>
    Var JavascriptFunction::CallFunction(RecyclableObject* function, JavascriptMethod entryPoint, Arguments args)
    {
        Js::Var varResult;

#if DBG && ENABLE_NATIVE_CODEGEN
        CheckIsExecutable(function, entryPoint);
#endif
        // compute size of stack to reserve
        CallInfo callInfo = args.Info;
        uint argsSize = callInfo.Count * sizeof(Var);

        ScriptContext * scriptContext = function->GetScriptContext();

        if (doStackProbe)
        {
            PROBE_STACK_CALL(scriptContext, function, argsSize);
        }

        void *data;
        void *savedEsp;
        __asm
        {
            // Save ESP
            mov savedEsp, esp
            mov eax, argsSize
            // Make sure we don't go beyond guard page
            cmp eax, 0x1000
            jge alloca_probe
            sub esp, eax
            jmp dbl_align
alloca_probe:
            // Use alloca to allocate more then a page size
            // Alloca assumes eax, contains size, and adjust ESP while
            // probing each page.
            call _alloca_probe_16
dbl_align:
            // 8-byte align frame to improve floating point perf of our JIT'd code.
            and esp, -8

            mov data, esp
        }

        {

            Var* dest = (Var*)data;
            Var* src = args.Values;
            for(unsigned int i =0; i < callInfo.Count; i++)
            {
                dest[i] = src[i];
            }
        }

        // call variable argument function provided in entryPoint
        __asm
        {
#ifdef _CONTROL_FLOW_GUARD
            // verify that the call target is valid
            mov  ecx, entryPoint
            call [__guard_check_icall_fptr]
            ; no need to restore ecx ('call entryPoint' is a __cdecl call)
#endif

            push callInfo
            push function
            call entryPoint

            // Restore ESP
            mov esp, savedEsp

            // save the return value from realsum.
            mov varResult, eax;
        }

        return varResult;
    }

#elif _M_X64
    template <bool doStackProbe>
    Var JavascriptFunction::CallFunction(RecyclableObject *function, JavascriptMethod entryPoint, Arguments args)
    {
        // compute size of stack to reserve and make sure we have enough stack.
        CallInfo callInfo = args.Info;
        uint argsSize = callInfo.Count * sizeof(Var);
        if (doStackProbe == true)
        {
            PROBE_STACK_CALL(function->GetScriptContext(), function, argsSize);
        }
#if DBG && ENABLE_NATIVE_CODEGEN
        CheckIsExecutable(function, entryPoint);
#endif
#ifdef _CONTROL_FLOW_GUARD
        _guard_check_icall((uintptr_t) entryPoint); /* check function pointer integrity */
#endif
        return amd64_CallFunction(function, entryPoint, args.Info, args.Info.Count, &args.Values[0]);
    }
#elif defined(_M_ARM)
    extern "C"
    {
        extern Var arm_CallFunction(JavascriptFunction* function, CallInfo info, Var* values, JavascriptMethod entryPoint);
    }

    template <bool doStackProbe>
    Var JavascriptFunction::CallFunction(RecyclableObject* function, JavascriptMethod entryPoint, Arguments args)
    {
        // compute size of stack to reserve and make sure we have enough stack.
        CallInfo callInfo = args.Info;
        uint argsSize = callInfo.Count * sizeof(Var);
        if (doStackProbe)
        {
            PROBE_STACK_CALL(function->GetScriptContext(), function, argsSize);
        }

#if DBG && ENABLE_NATIVE_CODEGEN
        CheckIsExecutable(function, entryPoint);
#endif
        Js::Var varResult;

        //The ARM can pass 4 arguments via registers so handle the cases for 0 or 1 values without resorting to asm code
        //(so that the asm code can assume 0 or more values will go on the stack: putting -1 values on the stack is unhealthy).
        unsigned count = args.Info.Count;
        if (count == 0)
        {
            varResult = entryPoint((JavascriptFunction*)function, args.Info);
        }
        else if (count == 1)
        {
            varResult = entryPoint((JavascriptFunction*)function, args.Info, args.Values[0]);
        }
        else
        {
            varResult = arm_CallFunction((JavascriptFunction*)function, args.Info, args.Values, entryPoint);
        }

        return varResult;
    }
#elif defined(_M_ARM64)
    extern "C"
    {
        extern Var arm64_CallFunction(JavascriptFunction* function, CallInfo info, Var* values, JavascriptMethod entryPoint);
    }

    template <bool doStackProbe>
    Var JavascriptFunction::CallFunction(RecyclableObject* function, JavascriptMethod entryPoint, Arguments args)
    {
        // compute size of stack to reserve and make sure we have enough stack.
        CallInfo callInfo = args.Info;
        uint argsSize = callInfo.Count * sizeof(Var);
        if (doStackProbe)
        {
            PROBE_STACK_CALL(function->GetScriptContext(), function, argsSize);
        }

#if DBG && ENABLE_NATIVE_CODEGEN
        CheckIsExecutable(function, entryPoint);
#endif
        Js::Var varResult;

        varResult = arm64_CallFunction((JavascriptFunction*)function, args.Info, args.Values, entryPoint);

        return varResult;
    }
#else
    Var JavascriptFunction::CallFunction(RecyclableObject *function, JavascriptMethod entryPoint, Arguments args)
    {
#if DBG && ENABLE_NATIVE_CODEGEN
        CheckIsExecutable(function, entryPoint);
#endif
#if 1
        Js::Throw::NotImplemented();
        return nullptr;
#else
        Var varResult;
        switch (info.Count)
        {
        case 0:
            {
                varResult=entryPoint((JavascriptFunction*)function, args.Info);
                break;
            }
        case 1: {
            varResult=entryPoint(
                (JavascriptFunction*)function,
                args.Info,
                args.Values[0]);
            break;
                }
        case 2: {
            varResult=entryPoint(
                (JavascriptFunction*)function,
                args.Info,
                args.Values[0],
                args.Values[1]);
            break;
                }
        case 3: {
            varResult=entryPoint(
                (JavascriptFunction*)function,
                args.Info,
                args.Values[0],
                args.Values[1],
                args.Values[2]);
            break;
                }
        case 4: {
            varResult=entryPoint(
                (JavascriptFunction*)function,
                args.Info,
                args.Values[0],
                args.Values[1],
                args.Values[2],
                args.Values[3]);
            break;
                }
        case 5: {
            varResult=entryPoint(
                (JavascriptFunction*)function,
                args.Info,
                args.Values[0],
                args.Values[1],
                args.Values[2],
                args.Values[3],
                args.Values[4]);
            break;
                }
        case 6: {
            varResult=entryPoint(
                (JavascriptFunction*)function,
                args.Info,
                args.Values[0],
                args.Values[1],
                args.Values[2],
                args.Values[3],
                args.Values[4],
                args.Values[5]);
            break;
                }
        case 7: {
            varResult=entryPoint(
                (JavascriptFunction*)function,
                args.Info,
                args.Values[0],
                args.Values[1],
                args.Values[2],
                args.Values[3],
                args.Values[4],
                args.Values[5],
                args.Values[6]);
            break;
                }
        case 8: {
            varResult=entryPoint(
                (JavascriptFunction*)function,
                args.Info,
                args.Values[0],
                args.Values[1],
                args.Values[2],
                args.Values[3],
                args.Values[4],
                args.Values[5],
                args.Values[6],
                args.Values[7]);
            break;
                }
        case 9: {
            varResult=entryPoint(
                (JavascriptFunction*)function,
                args.Info,
                args.Values[0],
                args.Values[1],
                args.Values[2],
                args.Values[3],
                args.Values[4],
                args.Values[5],
                args.Values[6],
                args.Values[7],
                args.Values[8]);
            break;
                }
        default:
            ScriptContext* scriptContext = function->type->GetScriptContext();
            varResult = scriptContext->GetLibrary()->GetUndefined();
            AssertMsg(false, "CallFunction call with unsupported number of arguments");
            break;
        }

#endif
        return varResult;
    }
#endif

    Var JavascriptFunction::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if (args.Info.Count == 0 || !JavascriptFunction::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedFunction, L"Function.prototype.toString");
        }
        JavascriptFunction *pFunc = JavascriptFunction::FromVar(args[0]);

        // pFunc can be from a different script context if Function.prototype.toString is invoked via .call/.apply.
        // Marshal the resulting string to the current script context (that of the toString)
        return CrossSite::MarshalVar(scriptContext, pFunc->EnsureSourceString());
    }

    JavascriptString* JavascriptFunction::GetNativeFunctionDisplayString(ScriptContext *scriptContext, JavascriptString *name)
    {
        return GetNativeFunctionDisplayStringCommon<JavascriptString>(scriptContext, name);
    }

    JavascriptString* JavascriptFunction::GetLibraryCodeDisplayString(ScriptContext *scriptContext, PCWSTR displayName)
    {
        return GetLibraryCodeDisplayStringCommon<JavascriptString, JavascriptString*>(scriptContext, displayName);
    }

#ifdef _M_IX86
    // This code is enabled by the -checkAlignment switch.
    // It verifies that all of our JS frames are 8 byte aligned.
    // Our alignments is based on aligning the return address of the function.
    // Note that this test can fail when Javascript functions are called directly
    // from helper functions.  This could be fixed by making these calls through
    // CallFunction(), or by having the helper 8 byte align the frame itself before
    // the call.  A lot of these though are not dealing with floats, so the cost
    // of doing the 8 byte alignment would outweigh the benefit...
    __declspec (naked)
    void JavascriptFunction::CheckAlignment()
    {
        _asm
        {
            test esp, 0x4
            je   LABEL1
            ret
LABEL1:
            call Throw::InternalError
        }
    }
#else
    void JavascriptFunction::CheckAlignment()
    {
        // Note: in order to enable this on ARM, uncomment/fix code in LowerMD.cpp (LowerEntryInstr).
    }
#endif

    BOOL JavascriptFunction::IsNativeAddress(ScriptContext * scriptContext, void * codeAddr)
    {
#if ENABLE_NATIVE_CODEGEN
        return scriptContext->IsNativeAddress(codeAddr);
#else
        return false;
#endif
    }

    Js::JavascriptMethod JavascriptFunction::DeferredParse(ScriptFunction** functionRef)
    {
        BOOL fParsed;
        return Js::ScriptFunction::DeferredParseCore(functionRef, fParsed);
    }

    Js::JavascriptMethod JavascriptFunction::DeferredParseCore(ScriptFunction** functionRef, BOOL &fParsed)
    {
        // Do the actual deferred parsing and byte code generation, passing the new entry point to the caller.

        ParseableFunctionInfo* functionInfo = (*functionRef)->GetParseableFunctionInfo();
        FunctionBody* funcBody = nullptr;

        Assert(functionInfo);

        if (functionInfo->IsDeferredParseFunction())
        {
            funcBody = functionInfo->Parse(functionRef);
            fParsed = funcBody->IsFunctionParsed() ? TRUE : FALSE;

#if ENABLE_PROFILE_INFO
            // This is the first call to the function, ensure dynamic profile info
            funcBody->EnsureDynamicProfileInfo();
#endif
        }
        else
        {
            funcBody = functionInfo->GetFunctionBody();
            Assert(funcBody != nullptr);
            Assert(!funcBody->IsDeferredParseFunction());
        }

        DebugOnly(JavascriptMethod directEntryPoint = funcBody->GetDirectEntryPoint(funcBody->GetDefaultEntryPointInfo()));
        Assert(directEntryPoint != DefaultDeferredParsingThunk && directEntryPoint != ProfileDeferredParsingThunk);
        return (*functionRef)->UpdateUndeferredBody(funcBody);
    }

    void JavascriptFunction::ReparseAsmJsModule(ScriptFunction** functionRef)
    {
        ParseableFunctionInfo* functionInfo = (*functionRef)->GetParseableFunctionInfo();

        Assert(functionInfo);
        Assert(functionInfo->HasBody());
        functionInfo->GetFunctionBody()->AddDeferParseAttribute();
        functionInfo->GetFunctionBody()->ResetEntryPoint();
        functionInfo->GetFunctionBody()->ResetInParams();

        FunctionBody * funcBody = functionInfo->Parse(functionRef);

#if ENABLE_PROFILE_INFO
        // This is the first call to the function, ensure dynamic profile info
        funcBody->EnsureDynamicProfileInfo();
#endif

        (*functionRef)->UpdateUndeferredBody(funcBody);
    }

    // Thunk for handling calls to functions that have not had byte code generated for them.

#if _M_IX86
    __declspec(naked)
    Var JavascriptFunction::DeferredParsingThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        __asm
        {
            push ebp
            mov ebp, esp
            lea eax, [esp+8]                // load the address of the function os that if we need to box, we can patch it up
            push eax
            call JavascriptFunction::DeferredParse
#ifdef _CONTROL_FLOW_GUARD
            // verify that the call target is valid
            mov  ecx, eax
            call[__guard_check_icall_fptr]
            mov eax, ecx
#endif
            pop ebp
            jmp eax
        }
    }
#elif defined(_M_X64) || defined(_M_ARM32_OR_ARM64)
    //Do nothing: the implementation of JavascriptFunction::DeferredParsingThunk is declared (appropriately decorated) in
    // Library\amd64\javascriptfunctiona.asm
    // Library\arm\arm_DeferredParsingThunk.asm
    // Library\arm64\arm64_DeferredParsingThunk.asm
#else
    Var JavascriptFunction::DeferredParsingThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        Js::Throw::NotImplemented();
        return nullptr;
    }
#endif

    ConstructorCache* JavascriptFunction::EnsureValidConstructorCache()
    {
        Assert(this->constructorCache != nullptr);
        this->constructorCache = ConstructorCache::EnsureValidInstance(this->constructorCache, this->GetScriptContext());
        return this->constructorCache;
    }

    void JavascriptFunction::ResetConstructorCacheToDefault()
    {
        Assert(this->constructorCache != nullptr);

        if (!this->constructorCache->IsDefault())
        {
            this->constructorCache = &ConstructorCache::DefaultInstance;
        }
    }

    // Thunk for handling calls to functions that have not had byte code generated for them.

#if _M_IX86
    __declspec(naked)
    Var JavascriptFunction::DeferredDeserializeThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        __asm
        {
            push ebp
            mov ebp, esp
            push [esp+8]
            call JavascriptFunction::DeferredDeserialize
#ifdef _CONTROL_FLOW_GUARD
            // verify that the call target is valid
            mov  ecx, eax
            call[__guard_check_icall_fptr]
            mov eax, ecx
#endif
            pop ebp
            jmp eax
        }
    }
#elif defined(_M_X64) || defined(_M_ARM32_OR_ARM64)
    //Do nothing: the implementation of JavascriptFunction::DeferredParsingThunk is declared (appropriately decorated) in
    // Library\amd64\javascriptfunctiona.asm
    // Library\arm\arm_DeferredParsingThunk.asm
    // Library\arm64\arm64_DeferredParsingThunk.asm
#else
    Var JavascriptFunction::DeferredDeserializeThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        Js::Throw::NotImplemented();
        return nullptr;
    }
#endif

    Js::JavascriptMethod JavascriptFunction::DeferredDeserialize(ScriptFunction* function)
    {
        FunctionInfo* funcInfo = function->GetFunctionInfo();
        Assert(funcInfo);
        FunctionBody* funcBody = nullptr;

        // If we haven't already deserialized this function, do so now
        // FunctionProxies could have gotten deserialized during the interpreter when
        // we tried to record the callsite info for the function which meant that it was a
        // target of a call. Or we could have deserialized the function info in another JavascriptFunctionInstance
        // In any case, fix up the function info if it's already been deserialized so that
        // we don't hold on to the proxy for too long, and rethunk it so that it directly
        // calls the default entry point the next time around
        if (funcInfo->IsDeferredDeserializeFunction())
        {
            DeferDeserializeFunctionInfo* deferDeserializeFunction = (DeferDeserializeFunctionInfo*) funcInfo;

            // This is the first call to the function, ensure dynamic profile info
            // Deserialize is a no-op if the function has already been deserialized
            funcBody = deferDeserializeFunction->Deserialize();
#if ENABLE_PROFILE_INFO
            funcBody->EnsureDynamicProfileInfo();
#endif
        }
        else
        {
            funcBody = funcInfo->GetFunctionBody();
            Assert(funcBody != nullptr);
            Assert(!funcBody->IsDeferredDeserializeFunction());
        }

        return function->UpdateUndeferredBody(funcBody);
    }
    void JavascriptFunction::SetEntryPoint(JavascriptMethod method)
    {
        this->GetDynamicType()->SetEntryPoint(method);
    }

    Var JavascriptFunction::EnsureSourceString()
    {
        return this->GetLibrary()->GetFunctionDisplayString();
    }

    /*
    *****************************************************************************************************************
                                Conditions checked by instruction decoder (In sequential order)
    ******************************************************************************************************************
    1)  Exception Code is AV i.e STATUS_ACCESS_VIOLATION
    2)  Check if Rip is Native address
    3)  Get the function object from RBP (a fixed offset from RBP) and check for the following
        a.  Not Null
        b.  Ensure that the function object is heap allocated
        c.  Ensure that the entrypointInfo is heap allocated
        d.  Ensure that the functionbody is heap allocated
        e.  Is a function
        f.  Is AsmJs Function object for asmjs
    4)  Check if Array BufferLength > 0x10000 (64K), power of 2 if length is less than 2^24 or multiple of 2^24  and multiple of 0x1000(4K) for asmjs
    5)  Check If the instruction is valid
        a.  Is one of the move instructions , i.e. mov, movsx, movzx, movsxd, movss or movsd
        b.  Get the array buffer register and its value for asmjs
        c.  Get the dst register(in case of load)
        d.  Calculate the number of bytes read in order to get the length of the instruction , ensure that the length should never be greater than 15 bytes
    6)  Check that the Array buffer value is same as the one we passed in EntryPointInfo in asmjs
    7)  Set the dst reg if the instr type is load
    8)  Add the bytes read to Rip and set it as new Rip
    9)  Return EXCEPTION_CONTINUE_EXECUTION

    */
#ifdef _M_X64
    ArrayAccessDecoder::InstructionData ArrayAccessDecoder::CheckValidInstr(BYTE* &pc, PEXCEPTION_POINTERS exceptionInfo, FunctionBody* funcBody) // get the reg operand and isLoad and
    {
        InstructionData instrData;
        uint prefixValue = 0;
        ArrayAccessDecoder::RexByteValue rexByteValue;
        bool isFloat = false;
        uint  immBytes = 0;
        uint dispBytes = 0;
        bool isImmediate = false;
        bool isSIB = false;
        // Read first  byte - check for prefix
        BYTE* beginPc = pc;
        if (((*pc) == 0x0F2) || ((*pc) == 0x0F3))
        {
            //MOVSD or MOVSS
            prefixValue = *pc;
            isFloat = true;
            pc++;
        }
        else if (*pc == 0x66)
        {
            prefixValue = *pc;
            pc++;
        }

        // Check for Rex Byte - After prefix we should have a rexByte if there is one
        if (*pc >= 0x40 && *pc <= 0x4F)
        {
            rexByteValue.rexValue = *pc;
            uint rexByte = *pc - 0x40;
            if (rexByte & 0x8)
            {
                rexByteValue.isW = true;
            }
            if (rexByte & 0x4)
            {
                rexByteValue.isR = true;
            }
            if (rexByte & 0x2)
            {
                rexByteValue.isX = true;
            }
            if (rexByte & 0x1)
            {
                rexByteValue.isB = true;
            }
            pc++;
        }

        // read opcode
        // Is one of the move instructions , i.e. mov, movsx, movzx, movsxd, movss or movsd
        switch (*pc)
        {
        //MOV - Store
        case 0x89:
        case 0x88:
        {
            pc++;
            instrData.isLoad = false;
            break;
        }
        //MOV - Load
        case 0x8A:
        case 0x8B:
        {
            pc++;
            instrData.isLoad = true;
            break;
        }
        case 0x0F:
        {
            // more than one byte opcode and hence we will read pc multiple times
            pc++;
            //MOVSX  , MOVSXD
            if (*pc == 0xBE || *pc == 0xBF)
            {
                instrData.isLoad = true;
            }
            //MOVZX
            else if (*pc == 0xB6 || *pc == 0xB7)
            {
                instrData.isLoad = true;
            }
            //MOVSS - Load
            else if (*pc == 0x10 && prefixValue == 0xF3)
            {
                Assert(isFloat);
                instrData.isLoad = true;
                instrData.isFloat32 = true;
            }
            //MOVSS - Store
            else if (*pc == 0x11 && prefixValue == 0xF3)
            {
                Assert(isFloat);
                instrData.isLoad = false;
                instrData.isFloat32 = true;
            }
            //MOVSD - Load
            else if (*pc == 0x10 && prefixValue == 0xF2)
            {
                Assert(isFloat);
                instrData.isLoad = true;
                instrData.isFloat64 = true;
            }
            //MOVSD - Store
            else if (*pc == 0x11 && prefixValue == 0xF2)
            {
                Assert(isFloat);
                instrData.isLoad = false;
                instrData.isFloat64 = true;
            }
            //MOVUPS - Load
            else if (*pc == 0x10 && prefixValue == 0)
            {
                instrData.isLoad = true;
                instrData.isSimd = true;
            }
            //MOVUPS - Store
            else if (*pc == 0x11 && prefixValue == 0)
            {
                instrData.isLoad = false;
                instrData.isSimd = true;
            }
            else
            {
                instrData.isInvalidInstr = true;
            }
            pc++;
            break;
        }
        // Support Mov Immediates
        // MOV
        case 0xC6:
        case 0xC7:
        {
            instrData.isLoad = false;
            instrData.isFloat64 = false;
            isImmediate = true;
            if (*pc == 0xC6)
            {
                immBytes = 1;
            }
            else if (rexByteValue.isW) // For MOV, REX.W set means we have a 32 bit immediate value, which gets extended to 64 bit.
            {
                immBytes = 4;
            }
            else
            {
                if (prefixValue == 0x66)
                {
                    immBytes = 2;
                }
                else
                {
                    immBytes = 4;
                }
            }
            pc++;
            break;
        }

        default:
            instrData.isInvalidInstr = true;
            break;
        }
        // if the opcode is not a move return
        if (instrData.isInvalidInstr)
        {
            return instrData;
        }

        //Read ModR/M
        // Read the Src Reg and also check for SIB
        // Add the isR bit to SrcReg and get the actual SRCReg
        // Get the number of bytes for displacement

        //get mod bits
        BYTE modVal = *pc & 0xC0; // first two bits(7th and 6th bits)
        modVal >>= 6;

        //get the R/M bits
        BYTE rmVal = (*pc) & 0x07; // last 3 bits ( 0,1 and 2nd bits)

        //get the reg value
        BYTE dstReg = (*pc) & 0x38; // mask reg bits (3rd 4th and 5th bits)
        dstReg >>= 3;

        Assert(dstReg <= 0x07);
        Assert(modVal <= 0x03);
        Assert(rmVal <= 0x07);

        switch (modVal)
        {
        case 0x00:
            dispBytes = 0;
            break;
        case 0x01:
            dispBytes = 1;
            break;
        case 0x02:
            dispBytes = 4;
            break;
        default:
            instrData.isInvalidInstr = true;
            break;
        }

        if (instrData.isInvalidInstr)
        {
            return instrData;
        }

        // Get the R/M value and see if SIB is present , else get the buffer reg
        if (rmVal == 0x04)
        {
            isSIB = true;
        }
        else
        {
            instrData.bufferReg = rmVal;
        }
        // Get the RegByes from ModRM

        instrData.dstReg = dstReg;

        // increment the modrm byte
        pc++;
        // Check if we have SIB and in that case bufferReg should not be set
        if (isSIB)
        {
            Assert(!instrData.bufferReg);
            // Get the Base and Index Reg from SIB and ensure that Scale is zero
            // We don't care about the Index reg
            // Add the isB value from Rex and get the actual Base Reg
            // Get the base register

            // 6f. Get the array buffer register and its value
            instrData.bufferReg = (*pc % 8);
            pc++;
        }
        // check for the Rex.B value and append it to the base register
        if (rexByteValue.isB)
        {
            instrData.bufferReg |= 1 << 3;
        }
        // check for the Rex.R value and append it to the dst register
        if (rexByteValue.isR)
        {
            instrData.dstReg |= 1 << 3;
        }

        // Get the buffer address - this is always 64 bit GPR
        switch (instrData.bufferReg)
        {
        case 0x0:
            instrData.bufferValue = exceptionInfo->ContextRecord->Rax;
            break;
        case 0x1:
            instrData.bufferValue = exceptionInfo->ContextRecord->Rcx;
            break;
        case 0x2:
            instrData.bufferValue = exceptionInfo->ContextRecord->Rdx;
            break;
        case 0x3:
            instrData.bufferValue = exceptionInfo->ContextRecord->Rbx;
            break;
        case 0x4:
            instrData.bufferValue = exceptionInfo->ContextRecord->Rsp;
            break;
        case 0x5:
            // RBP wouldn't point to an array buffer
            instrData.bufferValue = NULL;
            break;
        case 0x6:
            instrData.bufferValue = exceptionInfo->ContextRecord->Rsi;
            break;
        case 0x7:
            instrData.bufferValue = exceptionInfo->ContextRecord->Rdi;
            break;
        case 0x8:
            instrData.bufferValue = exceptionInfo->ContextRecord->R8;
            break;
        case 0x9:
            instrData.bufferValue = exceptionInfo->ContextRecord->R9;
            break;
        case 0xA:
            instrData.bufferValue = exceptionInfo->ContextRecord->R10;
            break;
        case 0xB:
            instrData.bufferValue = exceptionInfo->ContextRecord->R11;
            break;
        case 0xC:
            instrData.bufferValue = exceptionInfo->ContextRecord->R12;
            break;
        case 0xD:
            instrData.bufferValue = exceptionInfo->ContextRecord->R13;
            break;
        case 0xE:
            instrData.bufferValue = exceptionInfo->ContextRecord->R14;
            break;
        case 0xF:
            instrData.bufferValue = exceptionInfo->ContextRecord->R15;
            break;
        default:
            instrData.isInvalidInstr = true;
            Assert(false);// should never reach here as validation is done before itself
            return instrData;
        }
        // add the pc for displacement , we don't need the displacement Byte value
        if (dispBytes > 0)
        {
            pc = pc + dispBytes;
        }
        instrData.instrSizeInByte = (uint)(pc - beginPc);
        if (isImmediate)
        {
            Assert(immBytes > 0);
            instrData.instrSizeInByte += immBytes;
        }
        // Calculate the number of bytes read in order to get the length of the instruction , ensure that the length should never be greater than 15 bytes
        if (instrData.instrSizeInByte > 15)
        {
            // no instr size can be greater than 15
            instrData.isInvalidInstr = true;
        }
        return instrData;
    }

    int JavascriptFunction::ResumeForOutOfBoundsArrayRefs(int exceptionCode, PEXCEPTION_POINTERS exceptionInfo)
    {
#if ENABLE_NATIVE_CODEGEN
        if (exceptionCode != STATUS_ACCESS_VIOLATION)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();

        // AV should come from JITed code, since we don't eliminate bound checks in interpreter
        if (!threadContext->IsNativeAddress((Var)exceptionInfo->ContextRecord->Rip))
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        Var* addressOfFuncObj = (Var*)(exceptionInfo->ContextRecord->Rbp + 2 * sizeof(Var));
        if (!addressOfFuncObj)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        Js::ScriptFunction* func = (ScriptFunction::Is(*addressOfFuncObj))?(Js::ScriptFunction*)(*addressOfFuncObj):nullptr;
        if (!func)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        RecyclerHeapObjectInfo heapObject;
        Recycler* recycler = threadContext->GetRecycler();

        bool isFuncObjHeapAllocated = recycler->FindHeapObject(func, FindHeapObjectFlags_NoFlags, heapObject); // recheck if this needs to be removed
        bool isEntryPointHeapAllocated = recycler->FindHeapObject(func->GetEntryPointInfo(), FindHeapObjectFlags_NoFlags, heapObject);
        bool isFunctionBodyHeapAllocated = recycler->FindHeapObject(func->GetFunctionBody(), FindHeapObjectFlags_NoFlags, heapObject);

        // ensure that all our objects are heap allocated
        if (!(isFuncObjHeapAllocated && isEntryPointHeapAllocated && isFunctionBodyHeapAllocated))
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        bool isAsmJs = func->GetFunctionBody()->GetIsAsmJsFunction();
        Js::FunctionBody* funcBody = func->GetFunctionBody();
        BYTE* buffer = nullptr;
        if (isAsmJs)
        {
            // some extra checks for asm.js because we have slightly more information that we can validate
            Js::EntryPointInfo* entryPointInfo = (Js::EntryPointInfo*)funcBody->GetDefaultEntryPointInfo();
            uintptr moduleMemory = entryPointInfo->GetModuleAddress();
            if (!moduleMemory)
            {
                return EXCEPTION_CONTINUE_SEARCH;
            }
            ArrayBuffer* arrayBuffer = *(ArrayBuffer**)(moduleMemory + AsmJsModuleMemory::MemoryTableBeginOffset);
            if (!arrayBuffer || !arrayBuffer->GetBuffer())
            {
                // don't have a heap buffer for asm.js... so this shouldn't be an asm.js heap access
                return EXCEPTION_CONTINUE_SEARCH;
            }
            buffer = arrayBuffer->GetBuffer();

            uint bufferLength = arrayBuffer->GetByteLength();

            if (!arrayBuffer->IsValidAsmJsBufferLength(bufferLength))
            {
                return EXCEPTION_CONTINUE_SEARCH;
            }
        }

        BYTE* pc = (BYTE*)exceptionInfo->ExceptionRecord->ExceptionAddress;
        ArrayAccessDecoder::InstructionData instrData = ArrayAccessDecoder::CheckValidInstr(pc, exceptionInfo, funcBody);
        // Check If the instruction is valid
        if (instrData.isInvalidInstr)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        // If we didn't find the array buffer, ignore
        if (!instrData.bufferValue)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        // If asm.js, make sure the base address is that of the heap buffer
        if (isAsmJs && (instrData.bufferValue != (uint64)buffer))
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        // SIMD loads/stores do bounds checks.
        if (instrData.isSimd)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        // Set the dst reg if the instr type is load
        if (instrData.isLoad)
        {
            Var exceptionInfoReg = exceptionInfo->ContextRecord;
            Var* exceptionInfoIntReg = (Var*)((uint64)exceptionInfoReg + offsetof(CONTEXT, CONTEXT::Rax)); // offset in the contextRecord for RAX , the assert below checks for any change in the exceptionInfo struct
            Var* exceptionInfoFloatReg = (Var*)((uint64)exceptionInfoReg + offsetof(CONTEXT, CONTEXT::Xmm0));// offset in the contextRecord for XMM0 , the assert below checks for any change in the exceptionInfo struct
            Assert((DWORD64)*exceptionInfoIntReg == exceptionInfo->ContextRecord->Rax);
            Assert((uint64)*exceptionInfoFloatReg == exceptionInfo->ContextRecord->Xmm0.Low);

            if (instrData.isLoad)
            {
                double nanVal = JavascriptNumber::NaN;
                if (instrData.isFloat64)
                {
                    double* destRegLocation = (double*)((uint64)exceptionInfoFloatReg + 16 * (instrData.dstReg));
                    *destRegLocation = nanVal;
                }
                else if (instrData.isFloat32)
                {
                    float* destRegLocation = (float*)((uint64)exceptionInfoFloatReg + 16 * (instrData.dstReg));
                    *destRegLocation = (float)nanVal;
                }
                else
                {
                    uint64* destRegLocation = (uint64*)((uint64)exceptionInfoIntReg + 8 * (instrData.dstReg));
                    *destRegLocation = 0;
                }
            }
        }
        // Add the bytes read to Rip and set it as new Rip
        exceptionInfo->ContextRecord->Rip = exceptionInfo->ContextRecord->Rip + instrData.instrSizeInByte;

        return EXCEPTION_CONTINUE_EXECUTION;
#else
        return EXCEPTION_CONTINUE_SEARCH;
#endif
    }
#endif
#if DBG
    void JavascriptFunction::VerifyEntryPoint()
    {
        JavascriptMethod callEntryPoint = this->GetType()->GetEntryPoint();
        if (this->IsCrossSiteObject())
        {
            Assert(CrossSite::IsThunk(callEntryPoint));
        }
        else if (ScriptFunction::Is(this))
        {
        }
        else
        {
            JavascriptMethod originalEntryPoint = this->GetFunctionInfo()->GetOriginalEntryPoint();
            Assert(callEntryPoint == originalEntryPoint || callEntryPoint == ProfileEntryThunk
                || (this->GetScriptContext()->GetHostScriptContext()
                    && this->GetScriptContext()->GetHostScriptContext()->IsHostCrossSiteThunk(callEntryPoint))
                );
        }
    }
#endif

    /*static*/
    PropertyId const JavascriptFunction::specialPropertyIds[] =
    {
        PropertyIds::caller,
        PropertyIds::arguments
    };

    bool JavascriptFunction::HasRestrictedProperties() const
    {
        return !(this->functionInfo->IsClassMethod() || this->functionInfo->IsClassConstructor() || this->functionInfo->IsLambda() || this->IsGeneratorFunction() || this->IsBoundFunction() || this->IsStrictMode());
    }

    BOOL JavascriptFunction::HasProperty(PropertyId propertyId)
    {
        switch (propertyId)
        {
        case PropertyIds::caller:
        case PropertyIds::arguments:
            if (this->HasRestrictedProperties())
            {
                return true;
            }
            break;
        case PropertyIds::length:
            if (this->IsScriptFunction())
            {
                return true;
            }
            break;
        }
        return DynamicObject::HasProperty(propertyId);
    }

    BOOL JavascriptFunction::GetAccessors(PropertyId propertyId, Var *getter, Var *setter, ScriptContext * requestContext)
    {
        Assert(!this->IsBoundFunction());
        Assert(propertyId != Constants::NoProperty);
        Assert(getter);
        Assert(setter);
        Assert(requestContext);

        if (this->HasRestrictedProperties())
        {
            switch (propertyId)
            {
            case PropertyIds::caller:
                if (this->GetEntryPoint() == JavascriptFunction::PrototypeEntryPoint)
                {
                    *setter = *getter = requestContext->GetLibrary()->GetThrowTypeErrorCallerAccessorFunction();
                    return true;
                }
                break;

            case PropertyIds::arguments:
                if (this->GetEntryPoint() == JavascriptFunction::PrototypeEntryPoint)
                {
                    *setter = *getter = requestContext->GetLibrary()->GetThrowTypeErrorArgumentsAccessorFunction();
                    return true;
                }
                break;
            }
        }

        return __super::GetAccessors(propertyId, getter, setter, requestContext);
    }

    DescriptorFlags JavascriptFunction::GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        DescriptorFlags flags;
        if (GetSetterBuiltIns(propertyId, setterValue, info, requestContext, &flags))
        {
            return flags;
        }

        return __super::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags JavascriptFunction::GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        DescriptorFlags flags;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetSetterBuiltIns(propertyRecord->GetPropertyId(), setterValue, info, requestContext, &flags))
        {
            return flags;
        }

        return __super::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    bool JavascriptFunction::GetSetterBuiltIns(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext, DescriptorFlags* descriptorFlags)
    {
        Assert(propertyId != Constants::NoProperty);
        Assert(setterValue);
        Assert(requestContext);

        switch (propertyId)
        {
        case PropertyIds::caller:
            if (this->HasRestrictedProperties()) {
                PropertyValueInfo::SetNoCache(info, this);
                if (this->GetEntryPoint() == JavascriptFunction::PrototypeEntryPoint)
                {
                    *setterValue = requestContext->GetLibrary()->GetThrowTypeErrorCallerAccessorFunction();
                    *descriptorFlags = Accessor;
                }
                else
                {
                    *descriptorFlags = Data;
                }
                return true;
            }
            break;

        case PropertyIds::arguments:
            if (this->HasRestrictedProperties()) {
                PropertyValueInfo::SetNoCache(info, this);
                if (this->GetEntryPoint() == JavascriptFunction::PrototypeEntryPoint)
                {
                    *setterValue = requestContext->GetLibrary()->GetThrowTypeErrorArgumentsAccessorFunction();
                    *descriptorFlags = Accessor;
                }
                else
                {
                    *descriptorFlags = Data;
                }
                return true;
            }
            break;
        }

        return false;
    }

    BOOL JavascriptFunction::IsConfigurable(PropertyId propertyId)
    {
        if (DynamicObject::GetPropertyIndex(propertyId) == Constants::NoSlot)
        {
            switch (propertyId)
            {
            case PropertyIds::caller:
            case PropertyIds::arguments:
                if (this->HasRestrictedProperties())
                {
                    return false;
                }
                break;
            case PropertyIds::length:
                if (this->IsScriptFunction() || this->IsBoundFunction())
                {
                    return true;
                }
                break;
            }
        }
        return DynamicObject::IsConfigurable(propertyId);
    }

    BOOL JavascriptFunction::IsEnumerable(PropertyId propertyId)
    {
        if (DynamicObject::GetPropertyIndex(propertyId) == Constants::NoSlot)
        {
            switch (propertyId)
            {
            case PropertyIds::caller:
            case PropertyIds::arguments:
                if (this->HasRestrictedProperties())
                {
                    return false;
                }
                break;
            case PropertyIds::length:
                if (this->IsScriptFunction())
                {
                    return false;
                }
                break;
            }
        }
        return DynamicObject::IsEnumerable(propertyId);
    }

    BOOL JavascriptFunction::IsWritable(PropertyId propertyId)
    {
        if (DynamicObject::GetPropertyIndex(propertyId) == Constants::NoSlot)
        {
            switch (propertyId)
            {
            case PropertyIds::caller:
            case PropertyIds::arguments:
                if (this->HasRestrictedProperties())
                {
                    return false;
                }
                break;
            case PropertyIds::length:
                if (this->IsScriptFunction())
                {
                    return false;
                }
                break;
            }
        }
        return DynamicObject::IsWritable(propertyId);
    }

    BOOL JavascriptFunction::GetSpecialPropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext)
    {
        uint length = GetSpecialPropertyCount();
        if (index < length)
        {
            Assert(DynamicObject::GetPropertyIndex(specialPropertyIds[index]) == Constants::NoSlot);
            *propertyName = requestContext->GetPropertyString(specialPropertyIds[index]);
            return true;
        }

        if (index == length)
        {
            if (this->IsScriptFunction() || this->IsBoundFunction())
            {
                if (DynamicObject::GetPropertyIndex(PropertyIds::length) == Constants::NoSlot)
                {
                    //Only for user defined functions length is a special property.
                    *propertyName = requestContext->GetPropertyString(PropertyIds::length);
                    return true;
                }
            }
        }
        return false;
    }

    // Returns the number of special non-enumerable properties this type has.
    uint JavascriptFunction::GetSpecialPropertyCount() const
    {
        return this->HasRestrictedProperties() ? _countof(specialPropertyIds) : 0;
    }

    // Returns the list of special non-enumerable properties for the type.
    PropertyId const * JavascriptFunction::GetSpecialPropertyIds() const
    {
        return specialPropertyIds;
    }

    BOOL JavascriptFunction::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return JavascriptFunction::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    JavascriptFunction* JavascriptFunction::FindCaller(BOOL* foundThis, JavascriptFunction* nullValue, ScriptContext* requestContext)
    {
        ScriptContext* scriptContext = this->GetScriptContext();

        JavascriptFunction* funcCaller = nullValue;
        JavascriptStackWalker walker(scriptContext);

        if (walker.WalkToTarget(this))
        {
            *foundThis = TRUE;
            while (walker.GetCaller(&funcCaller))
            {
                if (walker.IsCallerGlobalFunction())
                {
                    // Caller is global/eval. If it's eval, keep looking.
                    // Otherwise, return null.
                    if (walker.IsEvalCaller())
                    {
                        continue;
                    }
                    funcCaller = nullValue;
                }
                break;
            }

            if (funcCaller->GetScriptContext() != requestContext && funcCaller->GetTypeId() == TypeIds_Null)
            {
                // There are cases where StackWalker might return null value from different scriptContext
                // Caller of this function expects nullValue from the requestContext.
                funcCaller = nullValue;
            }
        }

        return StackScriptFunction::EnsureBoxed(BOX_PARAM(funcCaller, nullptr, L"caller"));
    }

    BOOL JavascriptFunction::GetCallerProperty(Var originalInstance, Var* value, ScriptContext* requestContext)
    {
        ScriptContext* scriptContext = this->GetScriptContext();

        if (this->IsStrictMode())
        {
            return false;
        }

        if (this->GetEntryPoint() == JavascriptFunction::PrototypeEntryPoint)
        {
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {
                JavascriptFunction* accessor = requestContext->GetLibrary()->GetThrowTypeErrorCallerAccessorFunction();
                *value = accessor->GetEntryPoint()(accessor, 1, originalInstance);
            }
            return true;
        }

        JavascriptFunction* nullValue = (JavascriptFunction*)requestContext->GetLibrary()->GetNull();
        if (this->IsLibraryCode()) // Hide .caller for builtins
        {
            *value = nullValue;
            return true;
        }

        // Use a stack walker to find this function's frame. If we find it, find its caller.
        BOOL foundThis = FALSE;
        JavascriptFunction* funcCaller = FindCaller(&foundThis, nullValue, requestContext);

        // WOOB #1142373. We are trying to get the caller in window.onerror = function(){alert(arguments.callee.caller);} case
        // window.onerror is called outside of JavascriptFunction::CallFunction loop, so the caller information is not available
        // in the stack to be found by the stack walker.
        // As we had already walked the stack at throw time retrieve the caller information stored in the exception object
        // The down side is that we can only find the top level caller at thrown time, and won't be able to find caller.caller etc.
        // We'll try to fetch the caller only if we can find the function on the stack, but we can't find the caller if and we are in
        // window.onerror scenario.
        *value = funcCaller;
        if (foundThis && funcCaller == nullValue && scriptContext->GetThreadContext()->HasUnhandledException())
        {
            Js::JavascriptExceptionObject* unhandledExceptionObject = scriptContext->GetThreadContext()->GetUnhandledExceptionObject();
            if (unhandledExceptionObject)
            {
                JavascriptFunction* exceptionFunction = unhandledExceptionObject->GetFunction();
                // This is for getcaller in window.onError. The behavior is different in different browsers
                if (exceptionFunction && scriptContext == exceptionFunction->GetScriptContext())
                {
                    *value = exceptionFunction;
                }
            }
        }
        else if (foundThis && scriptContext != funcCaller->GetScriptContext())
        {
            HRESULT hr = scriptContext->GetHostScriptContext()->CheckCrossDomainScriptContext(funcCaller->GetScriptContext());
            if (S_OK != hr)
            {
                *value = scriptContext->GetLibrary()->GetNull();
            }
        }

        if (Js::JavascriptFunction::Is(*value) && Js::JavascriptFunction::FromVar(*value)->IsStrictMode())
        {
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {
                // ES5.15.3.5.4 [[Get]] (P) -- access to the 'caller' property of strict mode function results in TypeError.
                // Note that for caller coming from remote context (see the check right above) we can't call IsStrictMode()
                // unless CheckCrossDomainScriptContext succeeds. If it fails we don't know whether caller is strict mode
                // function or not and throw if it's not, so just return Null.
                JavascriptError::ThrowTypeError(scriptContext, JSERR_AccessCallerRestricted);
            }
        }

        return true;
    }

    BOOL JavascriptFunction::GetArgumentsProperty(Var originalInstance, Var* value, ScriptContext* requestContext)
    {
        ScriptContext* scriptContext = this->GetScriptContext();

        if (this->IsStrictMode())
        {
            return false;
        }

        if (this->GetEntryPoint() == JavascriptFunction::PrototypeEntryPoint)
        {
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {
                JavascriptFunction* accessor = requestContext->GetLibrary()->GetThrowTypeErrorArgumentsAccessorFunction();
                *value = accessor->GetEntryPoint()(accessor, 1, originalInstance);
            }
            return true;
        }

        if (!this->IsScriptFunction())
        {
            // builtin function do not have an argument object - return null.
            *value = scriptContext->GetLibrary()->GetNull();
            return true;
        }

        // Use a stack walker to find this function's frame. If we find it, compute its arguments.
        // Note that we are currently unable to guarantee that the binding between formal arguments
        // and foo.arguments[n] will be maintained after this object is returned.

        JavascriptStackWalker walker(scriptContext);

        if (walker.WalkToTarget(this))
        {
            if (walker.IsCallerGlobalFunction())
            {
                *value = requestContext->GetLibrary()->GetNull();
            }
            else
            {
                Var args = walker.GetPermanentArguments();

                if (args == NULL)
                {
                    CallInfo const *callInfo = walker.GetCallInfo();
                    args = JavascriptOperators::LoadHeapArguments(
                        this, callInfo->Count - 1,
                        walker.GetJavascriptArgs(),
                        scriptContext->GetLibrary()->GetNull(),
                        scriptContext->GetLibrary()->GetNull(),
                        scriptContext,
                        /* formalsAreLetDecls */ false);

                    walker.SetPermanentArguments(args);
                }

                *value = args;
            }
        }
        else
        {
            *value = scriptContext->GetLibrary()->GetNull();
        }
        return true;
    }

    BOOL JavascriptFunction::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        BOOL result = DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);

        if (result)
        {
            if (propertyId == PropertyIds::prototype)
            {
                PropertyValueInfo::DisableStoreFieldCache(info);
            }
        }
        else
        {
            GetPropertyBuiltIns(originalInstance, propertyId, value, requestContext, &result);
        }

        return result;
    }

    BOOL JavascriptFunction::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        result = DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
        if (result)
        {
            if (propertyRecord != nullptr && propertyRecord->GetPropertyId() == PropertyIds::prototype)
            {
                PropertyValueInfo::DisableStoreFieldCache(info);
            }

            return result;
        }

        if (propertyRecord != nullptr)
        {
            GetPropertyBuiltIns(originalInstance, propertyRecord->GetPropertyId(), value, requestContext, &result);
        }

        return result;
    }

    bool JavascriptFunction::GetPropertyBuiltIns(Var originalInstance, PropertyId propertyId, Var* value, ScriptContext* requestContext, BOOL* result)
    {
        if (propertyId == PropertyIds::caller && this->HasRestrictedProperties())
        {
            *result = GetCallerProperty(originalInstance, value, requestContext);
            return true;
        }

        if (propertyId == PropertyIds::arguments && this->HasRestrictedProperties())
        {
            *result = GetArgumentsProperty(originalInstance, value, requestContext);
            return true;
        }

        if (propertyId == PropertyIds::length)
        {
            FunctionProxy *proxy = this->GetFunctionProxy();
            if (proxy)
            {
                *value = TaggedInt::ToVarUnchecked(proxy->EnsureDeserialized()->GetReportedInParamsCount() - 1);
                *result = true;
                return true;
            }
        }

        return false;
    }

    BOOL JavascriptFunction::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        bool isReadOnly = false;
        switch (propertyId)
        {
        case PropertyIds::caller:
            if (this->HasRestrictedProperties())
            {
                isReadOnly = true;
            }
            break;

        case PropertyIds::arguments:
            if (this->HasRestrictedProperties())
            {
                isReadOnly = true;
            }
            break;

        case PropertyIds::length:
            if (this->IsScriptFunction())
            {
                isReadOnly = true;
            }
            break;

        }

        if (isReadOnly)
        {
            JavascriptError::ThrowCantAssignIfStrictMode(flags, this->GetScriptContext());
            return false;
        }

        BOOL result = DynamicObject::SetProperty(propertyId, value, flags, info);

        if (propertyId == PropertyIds::prototype)
        {
            PropertyValueInfo::SetNoCache(info, this);
            InvalidateConstructorCacheOnPrototypeChange();
            this->GetScriptContext()->GetThreadContext()->InvalidateIsInstInlineCachesForFunction(this);
        }

        return result;
    }

    BOOL JavascriptFunction::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        BOOL result = __super::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);

        if (propertyId == PropertyIds::prototype)
        {
            PropertyValueInfo::SetNoCache(info, this);
            InvalidateConstructorCacheOnPrototypeChange();
            this->GetScriptContext()->GetThreadContext()->InvalidateIsInstInlineCachesForFunction(this);
        }

        return result;
    }

    BOOL JavascriptFunction::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        PropertyRecord const * propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr)
        {
            return JavascriptFunction::SetProperty(propertyRecord->GetPropertyId(), value, flags, info);
        }
        else
        {
            return DynamicObject::SetProperty(propertyNameString, value, flags, info);
        }
    }

    BOOL JavascriptFunction::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {
        switch (propertyId)
        {
        case PropertyIds::caller:
        case PropertyIds::arguments:
            if (this->HasRestrictedProperties())
            {
                JavascriptError::ThrowCantDeleteIfStrictMode(flags, this->GetScriptContext(), this->GetScriptContext()->GetPropertyName(propertyId)->GetBuffer());
                return false;
            }
            break;
        case PropertyIds::length:
            if (this->IsScriptFunction())
            {
                JavascriptError::ThrowCantDeleteIfStrictMode(flags, this->GetScriptContext(), this->GetScriptContext()->GetPropertyName(propertyId)->GetBuffer());
                return false;
            }
            break;
        }

        BOOL result = DynamicObject::DeleteProperty(propertyId, flags);

        if (result && propertyId == PropertyIds::prototype)
        {
            InvalidateConstructorCacheOnPrototypeChange();
            this->GetScriptContext()->GetThreadContext()->InvalidateIsInstInlineCachesForFunction(this);
        }

        return result;
    }

    void JavascriptFunction::InvalidateConstructorCacheOnPrototypeChange()
    {
        Assert(this->constructorCache != nullptr);

#if DBG_DUMP
        if (PHASE_TRACE1(Js::ConstructorCachePhase))
        {
            // This is under DBG_DUMP so we can allow a check
            ParseableFunctionInfo* body = this->GetFunctionProxy() != nullptr ? this->GetFunctionProxy()->EnsureDeserialized() : nullptr;
            const wchar_t* ctorName = body != nullptr ? body->GetDisplayName() : L"<unknown>";

            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

            Output::Print(L"CtorCache: before invalidating cache (0x%p) for ctor %s (%s): ", this->constructorCache, ctorName,
                body ? body->GetDebugNumberSet(debugStringBuffer) : L"(null)");
            this->constructorCache->Dump();
            Output::Print(L"\n");
            Output::Flush();
        }
#endif

        this->constructorCache->InvalidateOnPrototypeChange();

#if DBG_DUMP
        if (PHASE_TRACE1(Js::ConstructorCachePhase))
        {
            // This is under DBG_DUMP so we can allow a check
            ParseableFunctionInfo* body = this->GetFunctionProxy() != nullptr ? this->GetFunctionProxy()->EnsureDeserialized() : nullptr;
            const wchar_t* ctorName = body != nullptr ? body->GetDisplayName() : L"<unknown>";
            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

            Output::Print(L"CtorCache: after invalidating cache (0x%p) for ctor %s (%s): ", this->constructorCache, ctorName,
                body ? body->GetDebugNumberSet(debugStringBuffer) : L"(null)");
            this->constructorCache->Dump();
            Output::Print(L"\n");
            Output::Flush();
        }
#endif

    }

    BOOL JavascriptFunction::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        JavascriptString * pString = NULL;

        Var sourceString = this->GetSourceString();

        if (sourceString == nullptr)
        {
            FunctionProxy* proxy = this->GetFunctionProxy();
            if (proxy)
            {
                ParseableFunctionInfo * func = proxy->EnsureDeserialized();
                Utf8SourceInfo* sourceInfo = func->GetUtf8SourceInfo();
                if (sourceInfo->GetIsLibraryCode())
                {
                    charcount_t displayNameLength = 0;
                    pString = JavascriptFunction::GetLibraryCodeDisplayString(this->GetScriptContext(), func->GetShortDisplayName(&displayNameLength));
                }
                else
                {
                    charcount_t count = min(DIAG_MAX_FUNCTION_STRING, func->LengthInChars());
                    utf8::DecodeOptions options = sourceInfo->IsCesu8() ? utf8::doAllowThreeByteSurrogates : utf8::doDefault;
                    utf8::DecodeInto(stringBuilder->AllocBufferSpace(count), func->GetSource(L"JavascriptFunction::GetDiagValueString"), count, options);
                    stringBuilder->IncreaseCount(count);
                    return TRUE;
                }
            }
            else
            {
                pString = GetLibrary()->GetFunctionDisplayString();
            }
        }
        else
        {
            if (TaggedInt::Is(sourceString))
            {
                pString = GetNativeFunctionDisplayString(this->GetScriptContext(), this->GetScriptContext()->GetPropertyString(TaggedInt::ToInt32(sourceString)));
            }
            else
            {
                Assert(JavascriptString::Is(sourceString));
                pString = JavascriptString::FromVar(sourceString);
            }
        }

        Assert(pString);
        stringBuilder->Append(pString->GetString(), pString->GetLength());

        return TRUE;
    }

    BOOL JavascriptFunction::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"Object, (Function)");
        return TRUE;
    }

    JavascriptString* JavascriptFunction::GetDisplayNameImpl() const
    {
        Assert(this->GetFunctionProxy() != nullptr); // The caller should guarantee a proxy exists
        ParseableFunctionInfo * func = this->GetFunctionProxy()->EnsureDeserialized();
        charcount_t length = 0;
        const wchar_t* name = func->GetShortDisplayName(&length);

        return DisplayNameHelper(name, length);
    }

    JavascriptString* JavascriptFunction::DisplayNameHelper(const wchar_t* name, charcount_t length) const
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        Assert(this->GetFunctionProxy() != nullptr); // The caller should guarantee a proxy exists
        ParseableFunctionInfo * func = this->GetFunctionProxy()->EnsureDeserialized();
        if (func->GetDisplayName() == Js::Constants::FunctionCode)
        {
            return LiteralString::NewCopyBuffer(Js::Constants::Anonymous, Js::Constants::AnonymousLength, scriptContext);
        }
        else if (func->GetIsAccessor())
        {
            const wchar_t* accessorName = func->GetDisplayName();
            if (accessorName[0] == L'g')
            {
                return LiteralString::Concat(LiteralString::NewCopySz(L"get ", scriptContext), LiteralString::NewCopyBuffer(name, length, scriptContext));
            }
            AssertMsg(accessorName[0] == L's', "should be a set");
            return LiteralString::Concat(LiteralString::NewCopySz(L"set ", scriptContext), LiteralString::NewCopyBuffer(name, length, scriptContext));
        }
        return LiteralString::NewCopyBuffer(name, length, scriptContext);
    }

    bool JavascriptFunction::GetFunctionName(JavascriptString** name) const
    {
        Assert(name != nullptr);
        FunctionProxy* proxy = this->GetFunctionProxy();
        JavascriptFunction* thisFunction = const_cast<JavascriptFunction*>(this);

        if (proxy || thisFunction->IsBoundFunction() || JavascriptGeneratorFunction::Is(thisFunction))
        {
            *name = GetDisplayNameImpl();
            return true;
        }

        Assert(!ScriptFunction::Is(thisFunction));
        return GetSourceStringName(name);
    }

    bool JavascriptFunction::GetSourceStringName(JavascriptString** name) const
    {
        Assert(name != nullptr);
        ScriptContext* scriptContext = this->GetScriptContext();
        Var sourceString = this->GetSourceString();

        if (sourceString)
        {
            if (TaggedInt::Is(sourceString))
            {
                int32 propertIdOfSourceString = TaggedInt::ToInt32(sourceString);
                *name = scriptContext->GetPropertyString(propertIdOfSourceString);
                return true;
            }
            Assert(JavascriptString::Is(sourceString));
            *name = JavascriptString::FromVar(sourceString);
            return true;
        }
        return false;
    }

    JavascriptString* JavascriptFunction::GetDisplayName() const
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        FunctionProxy* proxy = this->GetFunctionProxy();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        if (proxy)
        {
            ParseableFunctionInfo * func = proxy->EnsureDeserialized();
            return LiteralString::NewCopySz(func->GetDisplayName(), scriptContext);
        }
        JavascriptString* sourceStringName = nullptr;
        if (GetSourceStringName(&sourceStringName))
        {
            return sourceStringName;
        }

        return library->GetFunctionDisplayString();
    }

    Var JavascriptFunction::GetTypeOfString(ScriptContext * requestContext)
    {
        return requestContext->GetLibrary()->GetFunctionTypeDisplayString();
    }

    // Check if this function is native/script library code
    bool JavascriptFunction::IsLibraryCode() const
    {
        return !this->IsScriptFunction() || this->GetFunctionProxy()->GetUtf8SourceInfo()->GetIsLibraryCode();
    }

    // Implementation of Function.prototype[@@hasInstance](V) as specified in 19.2.3.6 of ES6 spec
    Var JavascriptFunction::EntrySymbolHasInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Function[Symbol.hasInstance]");
        }

        RecyclableObject * constructor = RecyclableObject::FromVar(args[0]);
        Var instance = args[1];
        if (!JavascriptConversion::IsCallable(constructor))
        {
            return JavascriptBoolean::ToVar(FALSE, scriptContext);
        }

        Assert(JavascriptProxy::Is(constructor) || JavascriptFunction::Is(constructor));
        return JavascriptBoolean::ToVar(constructor->HasInstance(instance, scriptContext, NULL), scriptContext);
    }

    BOOL JavascriptFunction::HasInstance(Var instance, ScriptContext* scriptContext, IsInstInlineCache* inlineCache)
    {
        Var funcPrototype;

        if (this->GetTypeHandler()->GetHasKnownSlot0())
        {
            Assert(this->GetDynamicType()->GetTypeHandler()->GetPropertyId(scriptContext, (PropertyIndex)0) == PropertyIds::prototype);
            funcPrototype = this->GetSlot(0);
        }
        else
        {
            funcPrototype = JavascriptOperators::GetProperty(this, PropertyIds::prototype, scriptContext, nullptr);
        }
        funcPrototype = CrossSite::MarshalVar(scriptContext, funcPrototype);
        return JavascriptFunction::HasInstance(funcPrototype, instance, scriptContext, inlineCache, this);
    }

    BOOL JavascriptFunction::HasInstance(Var funcPrototype, Var instance, ScriptContext * scriptContext, IsInstInlineCache* inlineCache, JavascriptFunction *function)
    {
        BOOL result = FALSE;
        JavascriptBoolean * javascriptResult;

        //
        // if "instance" is not a JavascriptObject, return false
        //
        if (!JavascriptOperators::IsObject(instance))
        {
            // Only update the cache for primitive cache if it is empty already for the JIT fast path
            if (inlineCache && inlineCache->function == nullptr
                && scriptContext == function->GetScriptContext())// only register when function has same scriptContext
            {
                inlineCache->Cache(RecyclableObject::Is(instance) ?
                    RecyclableObject::FromVar(instance)->GetType() : nullptr,
                    function, scriptContext->GetLibrary()->GetFalse(), scriptContext);
            }
            return result;
        }

        // If we have an instance of inline cache, let's try to use it to speed up the operation.
        // We would like to catch all cases when we already know (by having checked previously)
        // that an object on the left of instance of has been created by a function on the right,
        // as well as when we already know the object on the left has not been created by a function on the right.
        // In practice, we can do so only if the function matches the function in the cache, and the object's type matches the
        // type in the cache.  Notably, this typically means that if some of the objects evolved after construction,
        // while others did not, we will miss the cache for one of the two (sets of objects).
        // An important subtlety here arises when a function is called from different script contexts.
        // Suppose we called function foo from script context A, and we pass it an object o created in the same script context.
        // When function foo checks if object o is an instance of itself (function foo) for the first time (from context A) we will
        // populate the cache with function foo and object o's type (which is permanently bound to the script context A,
        // in which object o was created). If we later invoked function foo from script context B and perform the same instance-of check,
        // the function will still match the function in the cache (because objects' identities do not change during cross-context marshalling).
        // However, object o's type (even if it is of the same "shape" as before) will be different, because the object types are permanently
        // bound and unique to the script context from which they were created.  Hence, the cache may miss, even if the function matches.
        if (inlineCache != nullptr)
        {
            Assert(function != nullptr);
            if (inlineCache->TryGetResult(instance, function, &javascriptResult))
            {
                return javascriptResult == scriptContext->GetLibrary()->GetTrue();
            }
        }

        // If we are here, then me must have missed the cache.  This may be because:
        // a) the cache has never been populated in the first place,
        // b) the cache has been populated, but for an object of a different type (even if the object was created by the same constructor function),
        // c) the cache has been populated, but for a different function,
        // d) the cache has been populated, even for the same object type and function, but has since been invalidated, because the function's
        //    prototype property has been changed (see JavascriptFunction::SetProperty and ThreadContext::InvalidateIsInstInlineCachesForFunction).

        // We may even miss the cache if we ask again about the very same object the very same function the cache was populated with.
        // This subtlety arises when a function is called from two (or more) different script contexts.
        // Suppose we called function foo from script context A, and passed it an object o created in the same script context.
        // When function foo checks if object o is an instance of itself (function foo) for the first time (from context A) we will
        // populate the cache with function foo and object o's type (which is permanently bound to the script context A,
        // in which object o was created). If we later invoked function foo from script context B and perform the same instance of check,
        // the function will still match the function in the cache (because objects' identities do not change during cross-context marshalling).
        // However, object o's type (even if it is of the same "shape" as before, and even if o is the very same object) will be different,
        // because the object types are permanently bound and unique to the script context from which they were created.

        Var prototype = JavascriptOperators::GetPrototype(RecyclableObject::FromVar(instance));

        if (!JavascriptOperators::IsObject(funcPrototype))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidPrototype);
        }

        // Since we missed the cache, we must now walk the prototype chain of the object to check if the given function's prototype is somewhere in
        // that chain. If it is, we return true. Otherwise (i.e., we hit the end of the chain before finding the function's prototype) we return false.
        while (JavascriptOperators::GetTypeId(prototype) != TypeIds_Null)
        {
            if (prototype == funcPrototype)
            {
                result = TRUE;
                break;
            }

            prototype = JavascriptOperators::GetPrototype(RecyclableObject::FromVar(prototype));
        }

        // Now that we know the answer, let's cache it for next time if we have a cache.
        if (inlineCache != NULL)
        {
            Assert(function != NULL);
            JavascriptBoolean * boolResult = result ? scriptContext->GetLibrary()->GetTrue() :
                scriptContext->GetLibrary()->GetFalse();
            Type * instanceType = RecyclableObject::FromVar(instance)->GetType();

            if (!instanceType->HasSpecialPrototype()
                && scriptContext == function->GetScriptContext()) // only register when function has same scriptContext, otherwise when scriptContext close
                                                                  // and the isInst inline cache chain will be broken by clearing the arenaAllocator
            {
                inlineCache->Cache(instanceType, function, boolResult, scriptContext);
            }
        }

        return result;
    }
}

//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include <strsafe.h>
#include "ByteCode\ByteCodeAPI.h"
#include "Exceptions\EvalDisabledException.h"

#include "Types\PropertyIndexRanges.h"
#include "Types\SimpleDictionaryPropertyDescriptor.h"
#include "Types\SimpleDictionaryTypeHandler.h"

namespace Js
{
    GlobalObject * GlobalObject::New(ScriptContext * scriptContext)
    {
        SimpleDictionaryTypeHandler* globalTypeHandler = SimpleDictionaryTypeHandler::New(
            scriptContext->GetRecycler(), InitialCapacity, InlineSlotCapacity, sizeof(Js::GlobalObject));

        DynamicType* globalType = DynamicType::New(
            scriptContext, TypeIds_GlobalObject, nullptr, nullptr, globalTypeHandler);

        GlobalObject* globalObject = RecyclerNewPlus(scriptContext->GetRecycler(),
            sizeof(Var) * InlineSlotCapacity, GlobalObject, globalType, scriptContext);

        globalTypeHandler->SetSingletonInstanceIfNeeded(scriptContext->GetRecycler()->CreateWeakReferenceHandle<DynamicObject>(globalObject));

        return globalObject;
    }

    GlobalObject::GlobalObject(DynamicType * type, ScriptContext* scriptContext) :
        RootObjectBase(type, scriptContext),
        directHostObject(nullptr),
        secureDirectHostObject(nullptr),
        EvalHelper(&GlobalObject::DefaultEvalHelper),
        reservedProperties(nullptr)
    {
    }

    void GlobalObject::Initialize(ScriptContext * scriptContext)
    {
        Assert(type->javascriptLibrary == nullptr);
        JavascriptLibrary* localLibrary = RecyclerNewFinalized(scriptContext->GetRecycler(), JavascriptLibrary, this);
        scriptContext->SetLibrary(localLibrary);
        type->javascriptLibrary = localLibrary;
        localLibrary->Initialize(scriptContext, this);
        library = localLibrary;
    }

    bool GlobalObject::Is(Var aValue)
    {
        return RecyclableObject::Is(aValue) && (RecyclableObject::FromVar(aValue)->GetTypeId() == TypeIds_GlobalObject);
    }

    GlobalObject* GlobalObject::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'GlobalObject'");
        return static_cast<GlobalObject*>(aValue);
    }

    HRESULT GlobalObject::SetDirectHostObject(RecyclableObject* hostObject, RecyclableObject* secureDirectHostObject)
    {
        HRESULT hr = S_OK;

        BEGIN_TRANSLATE_OOM_TO_HRESULT_NESTED
        {
            // In fastDOM scenario, we should use the host object to lookup the prototype.
            this->SetPrototype(library->GetNull());
        }
        END_TRANSLATE_OOM_TO_HRESULT(hr)

        this->directHostObject = hostObject;
        this->secureDirectHostObject = secureDirectHostObject;
        return hr;
    }

    RecyclableObject* GlobalObject::GetDirectHostObject()
    {
        return this->directHostObject;
    }

    RecyclableObject* GlobalObject::GetSecureDirectHostObject()
    {
        return this->secureDirectHostObject;
    }

    // Converts the global object into the object that should be used as the 'this' parameter.
    // In a non-hosted environment, the global object (this) is returned.
    // In a hosted environment, the host object is returned.
    Var GlobalObject::ToThis()
    {
        Var ret;

        // In fast DOM, we need to give user the secure version of host object
        if (secureDirectHostObject)
        {
            ret = secureDirectHostObject;
        }
        else if (hostObject)
        {
            // If the global object has the host object, use that as "this"
            ret = hostObject->GetHostDispatchVar();
            Assert(ret);
        }
        else
        {
            // Otherwise just use the global object
            ret = this;
        }

        return ret;
    }

    BOOL GlobalObject::ReserveGlobalProperty(PropertyId propertyId)
    {
        if (DynamicObject::HasProperty(propertyId))
        {
            return false;
        }
        if (reservedProperties == nullptr)
        {
            Recycler* recycler = this->GetScriptContext()->GetRecycler();
            reservedProperties = RecyclerNew(recycler, ReservedPropertiesHashSet, recycler, 3);
        }
        reservedProperties->AddNew(propertyId);
        return true;
    }

    BOOL GlobalObject::IsReservedGlobalProperty(PropertyId propertyId)
    {
        return reservedProperties != nullptr && reservedProperties->Contains(propertyId);
    }

#ifdef IR_VIEWER
    Var GlobalObject::EntryParseIR(RecyclableObject *function, CallInfo callInfo, ...)
    {
        //
        // retrieve arguments
        //

        RUNTIME_ARGUMENTS(args, callInfo);
        Js::Var codeVar = args[1];  // args[0] is (this)

        Js::JavascriptString *codeStringVar = nullptr;
        const wchar_t *source = nullptr;
        size_t sourceLength = 0;

        if (Js::JavascriptString::Is(codeVar))
        {
            codeStringVar = (Js::JavascriptString *)codeVar;
            source = codeStringVar->GetString();
            sourceLength = codeStringVar->GetLength();
        }
        else
        {
            AssertMsg(false, "The input to parseIR was not a string.");
        }

        //
        // collect arguments for eval
        //

        /* @see NativeCodeGenerator::CodeGen */
        ScriptContext *scriptContext = function->GetScriptContext();

        // used arbitrary defaults for these, but it seems to work
        ModuleID moduleID = 0;
        ulong grfscr = 0;
        LPCOLESTR pszTitle = L"";
        BOOL registerDocument = false;

        BOOL strictMode = true;

        return IRDumpEvalHelper(scriptContext, source, sourceLength, moduleID, grfscr,
            pszTitle, registerDocument, FALSE, strictMode);
    }

    // TODO remove when refactor
    Js::PropertyId GlobalObject::CreateProperty(Js::ScriptContext *scriptContext, const wchar_t *propertyName)
    {
        Js::PropertyRecord const *propertyRecord;
        scriptContext->GetOrAddPropertyRecord(propertyName, (int) wcslen(propertyName), &propertyRecord);
        Js::PropertyId propertyId = propertyRecord->GetPropertyId();

        return propertyId;
    }

    // TODO remove when refactor
    void GlobalObject::SetProperty(Js::DynamicObject *obj, const wchar_t *propertyName, Js::Var value)
    {
        const size_t len = wcslen(propertyName);
        if (!(len > 0))
        {
            return;
        }

        Js::PropertyId id = CreateProperty(obj->GetScriptContext(), propertyName);
        SetProperty(obj, id, value);
    }

    // TODO remove when refactor
    void GlobalObject::SetProperty(Js::DynamicObject *obj, Js::PropertyId id, Js::Var value)
    {
        if (value == NULL)
        {
            return;
        }

        Js::JavascriptOperators::SetProperty(obj, obj, id, value, obj->GetScriptContext());
    }

    Var GlobalObject::FunctionInfoObjectBuilder(ScriptContext *scriptContext, const wchar_t *file,
        const wchar_t *function, ULONG lineNum, ULONG colNum,
        uint functionId, Js::Utf8SourceInfo *utf8SrcInfo, Js::Var source)
    {
        Js::DynamicObject *fnInfoObj = scriptContext->GetLibrary()->CreateObject();

        // create javascript objects for properties
        Js::Var filenameString = Js::JavascriptString::NewCopyBuffer(file, wcslen(file), scriptContext);
        Js::Var funcnameString = Js::JavascriptString::NewCopyBuffer(function, wcslen(function), scriptContext);
        Js::Var lineNumber = Js::JavascriptNumber::ToVar((int64) lineNum, scriptContext);
        Js::Var colNumber = Js::JavascriptNumber::ToVar((int64) colNum, scriptContext);
        Js::Var functionIdNumberVar = Js::JavascriptNumber::ToVar(functionId, scriptContext);
        Js::Var utf8SourceInfoVar = Js::JavascriptNumber::ToVar((long) utf8SrcInfo, scriptContext);

        // assign properties to function info object
        SetProperty(fnInfoObj, L"filename", filenameString);
        SetProperty(fnInfoObj, L"function", funcnameString);
        SetProperty(fnInfoObj, L"line", lineNumber);
        SetProperty(fnInfoObj, L"col", colNumber);
        SetProperty(fnInfoObj, L"funcId", functionIdNumberVar);
        SetProperty(fnInfoObj, L"utf8SrcInfoPtr", utf8SourceInfoVar);
        SetProperty(fnInfoObj, L"source", source);

        return fnInfoObj;
    }

    /**
     * Return a list of functions visible in the current ScriptContext.
     */
    Var GlobalObject::EntryFunctionList(RecyclableObject *function, CallInfo callInfo, ...)
    {
        // Note: the typedefs below help make the following code more readable

        // See: Js::ScriptContext::SourceList (declaration not visible from this file)
        typedef JsUtil::List<RecyclerWeakReference<Utf8SourceInfo>*, Recycler, false, Js::FreeListedRemovePolicy> SourceList;
        typedef RecyclerWeakReference<Js::Utf8SourceInfo> Utf8SourceInfoRef;

        ScriptContext *originalScriptContext = function->GetScriptContext();
        ThreadContext *threadContext = originalScriptContext->GetThreadContext();
        ScriptContext *scriptContext;

        int fnCount = 0;
        for (scriptContext = threadContext->GetScriptContextList();
            scriptContext;
            scriptContext = scriptContext->next)
        {
            if (scriptContext->IsClosed()) continue;

            //
            // get count of functions in all files in script context
            //
            SourceList *sourceList = scriptContext->GetSourceList();
            sourceList->Map([&fnCount](uint i, Utf8SourceInfoRef *sourceInfoWeakRef)
            {
                Js::Utf8SourceInfo *sourceInfo = sourceInfoWeakRef->Get();
                if (sourceInfo == nullptr || sourceInfo->GetIsLibraryCode()) // library code has no source, skip
                {
                    return;
                }

                fnCount += sourceInfo->GetFunctionBodyCount();
            });
        }

#ifdef ENABLE_IR_VIEWER_DBG_DUMP
        if (Js::Configuration::Global.flags.Verbose)
        {
            Output::Print(L">> There are %d total functions\n", fnCount);
            Output::Flush();
        }
#endif

        //
        // create a javascript array to hold info for all of the functions
        //
        Js::JavascriptArray *functionList = originalScriptContext->GetLibrary()->CreateArray(fnCount);

        int count = 0;
        for (scriptContext = threadContext->GetScriptContextList();
            scriptContext;
            scriptContext = scriptContext->next)
        {
            if (scriptContext->IsClosed()) continue;
            // if (scriptContext == originalScriptContext) continue;  // uncomment to ignore the originalScriptContext

            SourceList *sourceList = scriptContext->GetSourceList();
            sourceList->Map([&fnCount, &count, functionList, scriptContext](uint i, Utf8SourceInfoRef *sourceInfoWeakRef)
            {
                Js::Utf8SourceInfo *sourceInfo = sourceInfoWeakRef->Get();
                if (sourceInfo == nullptr || sourceInfo->GetIsLibraryCode()) // library code has no source, skip
                {
                    return;
                }

                const SRCINFO *srcInfo = sourceInfo->GetSrcInfo();
                SourceContextInfo *srcContextInfo = srcInfo->sourceContextInfo;

                //
                // get URL of source file
                //
                wchar_t filenameBuffer[128];  // hold dynamically built filename
                wchar_t const *srcFileUrl = NULL;
                if (!srcContextInfo->IsDynamic())
                {
                    Assert(srcContextInfo->url != NULL);
                    srcFileUrl = srcContextInfo->url;
                }
                else
                {
                    StringCchPrintf(filenameBuffer, 128, L"[dynamic(hash:%u)]", srcContextInfo->hash);
                    srcFileUrl = filenameBuffer;
                }

                sourceInfo->MapFunction([scriptContext, &count, functionList, srcFileUrl](Js::FunctionBody *functionBody)
                {
                    wchar_t const *functionName = functionBody->GetExternalDisplayName();

                    ULONG lineNum = functionBody->GetLineNumber();
                    ULONG colNum = functionBody->GetColumnNumber();

                    uint funcId = functionBody->GetLocalFunctionId();
                    Js::Utf8SourceInfo *utf8SrcInfo = functionBody->GetUtf8SourceInfo();
                    if (utf8SrcInfo == nullptr)
                    {
                        return;
                    }

                    Assert(utf8SrcInfo->FindFunction(funcId) == functionBody);

                    BufferStringBuilder builder(functionBody->LengthInChars(), scriptContext);
                    utf8::DecodeOptions options = utf8SrcInfo->IsCesu8() ? utf8::doAllowThreeByteSurrogates : utf8::doDefault;
                    utf8::DecodeInto(builder.DangerousGetWritableBuffer(), functionBody->GetSource(), functionBody->LengthInChars(), options);
                    Var cachedSourceString = builder.ToString();

                    Js::Var obj = FunctionInfoObjectBuilder(scriptContext, srcFileUrl,
                        functionName, lineNum, colNum, funcId, utf8SrcInfo, cachedSourceString);
                    functionList->SetItem(count, obj, Js::PropertyOperationFlags::PropertyOperation_None);

                    ++count;
                });
            });
        }

#ifdef ENABLE_IR_VIEWER_DBG_DUMP
        if (Js::Configuration::Global.flags.Verbose)
        {
            Output::Print(L">> Returning from functionList()\n");
            Output::Flush();
        }
#endif

        return functionList;
    }

    /**
     * Return the parsed IR for the given function.
     */
    Var GlobalObject::EntryRejitFunction(RecyclableObject *function, CallInfo callInfo, ...)
    {
#ifdef ENABLE_IR_VIEWER_DBG_DUMP
        if (Js::Configuration::Global.flags.Verbose)
        {
            Output::Print(L">> Entering rejitFunction()\n");
            Output::Flush();
        }
#endif

        //
        // retrieve and coerce arguments
        //

        RUNTIME_ARGUMENTS(args, callInfo);  // args[0] is (callInfo)
        Js::Var jsVarUtf8SourceInfo = args[1];
        Js::Var jsVarFunctionId = args[2];

#ifdef ENABLE_IR_VIEWER_DBG_DUMP
        if (Js::Configuration::Global.flags.Verbose)
        {
            Output::Print(L"jsVarUtf8SourceInfo: %d (0x%08X)\n", jsVarUtf8SourceInfo, jsVarUtf8SourceInfo);
            Output::Print(L"jsVarFunctionId: %d (0x%08X)\n", jsVarFunctionId, jsVarFunctionId);
        }
#endif

        Js::JavascriptNumber *jsUtf8SourceInfoNumber = NULL;
        Js::JavascriptNumber *jsFunctionIdNumber = NULL;
        long utf8SourceInfoNumber = 0;  // null
        long functionIdNumber = -1;  // start with invalid function id

        // extract value of jsVarUtf8SourceInfo
        if (Js::TaggedInt::Is(jsVarUtf8SourceInfo))
        {
            utf8SourceInfoNumber = (long)TaggedInt::ToInt64(jsVarUtf8SourceInfo); // REVIEW: just truncate?
        }
        else if (Js::JavascriptNumber::Is(jsVarUtf8SourceInfo))
        {
            jsUtf8SourceInfoNumber = (Js::JavascriptNumber *)jsVarUtf8SourceInfo;
            utf8SourceInfoNumber = (long)JavascriptNumber::GetValue(jsUtf8SourceInfoNumber);    // REVIEW: just truncate?
        }
        else
        {
            // should not reach here
            AssertMsg(false, "Failed to extract value for jsVarUtf8SourceInfo as either TaggedInt or JavascriptNumber.\n");
        }

        // extract value of jsVarFunctionId
        if (Js::TaggedInt::Is(jsVarFunctionId))
        {
            functionIdNumber = (long)TaggedInt::ToInt64(jsVarFunctionId); // REVIEW: just truncate?
        }
        else if (Js::JavascriptNumber::Is(jsVarFunctionId))
        {
            jsFunctionIdNumber = (Js::JavascriptNumber *)jsVarFunctionId;
            functionIdNumber = (long)JavascriptNumber::GetValue(jsFunctionIdNumber); // REVIEW: just truncate?
        }
        else
        {
            // should not reach here
            AssertMsg(false, "Failed to extract value for jsVarFunctionId as either TaggedInt or JavascriptNumber.\n");
        }

#ifdef ENABLE_IR_VIEWER_DBG_DUMP
        if (Js::Configuration::Global.flags.Verbose)
        {
            Output::Print(L"utf8SourceInfoNumber (value): %d (0x%08X)\n", utf8SourceInfoNumber, utf8SourceInfoNumber);
            Output::Print(L"jsVarFunctionId (value): %d (0x%08X)\n", functionIdNumber, functionIdNumber);
            Output::Print(L">> Executing rejitFunction(%d, %d)\n", utf8SourceInfoNumber, functionIdNumber);
            Output::Flush();
        }
#endif

        //
        // recover functionBody
        //

        Js::Utf8SourceInfo *sourceInfo = (Js::Utf8SourceInfo *)(utf8SourceInfoNumber);
        if (sourceInfo == NULL)
        {
            return NULL;
        }

        Js::FunctionBody *functionBody = sourceInfo->FindFunction((Js::LocalFunctionId)functionIdNumber);

        //
        // rejit the function body
        //

        Js::ScriptContext *scriptContext = function->GetScriptContext();
        NativeCodeGenerator *nativeCodeGenerator = scriptContext->GetNativeCodeGenerator();

        if (!functionBody || !functionBody->GetByteCode())
        {
            return scriptContext->GetLibrary()->GetUndefined();
        }

        Js::Var retVal = RejitIRViewerFunction(nativeCodeGenerator, functionBody, scriptContext);

        //
        // return the parsed IR object
        //

#ifdef ENABLE_IR_VIEWER_DBG_DUMP
        if (Js::Configuration::Global.flags.Verbose)
        {
            Output::Print(L"rejitFunction - retVal: 0x%08X\n", retVal);
            Output::Flush();
        }
#endif

        return retVal;
    }

#endif /* IR_VIEWER */
    Var GlobalObject::EntryEvalHelper(ScriptContext* scriptContext, RecyclableObject* function, CallInfo callInfo, Js::Arguments& args)
    {
        FrameDisplay* environment = (FrameDisplay*)&NullFrameDisplay;
        ModuleID moduleID = kmodGlobal;
        BOOL strictMode = FALSE;
        // TODO: Handle call from global scope, strict mode
        BOOL isIndirect = FALSE;

        if (args.Info.Flags & CallFlags_ExtraArg)
        {
            // This was recognized as an eval call at compile time. The last one or two args are internal to us.
            // Argcount will be one of the following when called from global code
            //  - eval("...")     : argcount 3 : this, evalString, frameDisplay
            //  - eval.call("..."): argcount 2 : this(which is string) , frameDisplay
            if (args.Info.Count >= 2)
            {
                environment = (FrameDisplay*)(args[args.Info.Count - 1]);

                // Check for a module root passed from the caller. If it's there, use its module ID to compile the eval.
                // when called inside a module root, module root would be added before the frame display in above scenarios

                // ModuleRoot is optional
                //  - eval("...")     : argcount 3/4 : this, evalString , [module root], frameDisplay
                //  - eval.call("..."): argcount 2/3 : this(which is string) , [module root], frameDisplay

                strictMode = environment->GetStrictMode();

                if (args.Info.Count >= 3 && JavascriptOperators::GetTypeId(args[args.Info.Count - 2]) == TypeIds_ModuleRoot)
                {
                    moduleID = ((Js::ModuleRoot*)(RecyclableObject::FromVar(args[args.Info.Count - 2])))->GetModuleID();
                    args.Info.Count--;
                }
                args.Info.Count--;
            }
        }
        else
        {
            // This must be an indirect "eval" call that we didn't detect at compile time.
            // Pass null as the environment, which will force all lookups in the eval code
            // to use the root for the current module.
            // Also pass "null" for "this", which will force the callee to use the current module root.
            isIndirect = !PHASE_OFF1(Js::FastIndirectEvalPhase);
        }

        return GlobalObject::VEval(function->GetLibrary(), environment, moduleID, !!strictMode, !!isIndirect, args,
            /* isLibraryCode = */ false, /* registerDocument */ true, /*additionalGrfscr */ 0);
    }

    Var GlobalObject::EntryEvalRestrictedMode(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        RUNTIME_ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptLibrary* library = function->GetLibrary();
        ScriptContext* scriptContext = library->GetScriptContext();

        scriptContext->CheckEvalRestriction();

        return EntryEvalHelper(scriptContext, function, callInfo, args);
    }

    // This function is used to decipher eval function parameters and we don't want the stack arguments optimization by C++ compiler so turning off the optimization
    Var GlobalObject::EntryEval(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        RUNTIME_ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptLibrary* library = function->GetLibrary();
        ScriptContext* scriptContext = library->GetScriptContext();

        return EntryEvalHelper(scriptContext, function, callInfo, args);
    }

    Var GlobalObject::VEval(JavascriptLibrary* library, FrameDisplay* environment, ModuleID moduleID, bool strictMode, bool isIndirect,
        Arguments& args, bool isLibraryCode, bool registerDocument, ulong additionalGrfscr)
    {
        Assert(library);
        ScriptContext* scriptContext = library->GetScriptContext();

        unsigned argCount = args.Info.Count;

        AssertMsg(argCount > 0, "Should always have implicit 'this'");

        bool doRegisterDocument = registerDocument & !isLibraryCode;
        Var varThis = library->GetUndefined();

        if (argCount < 2)
        {
            return library->GetUndefined();
        }

        Var evalArg = args[1];
        if (!JavascriptString::Is(evalArg))
        {
            // "If x is not a string value, return x."
            return evalArg;
        }

        // It might happen that no script parsed on this context (scriptContext) till now,
        // so this Eval acts as the first source compile for scriptContext, transition to debugMode as needed
        scriptContext->TransitionToDebugModeIfFirstSource(/* utf8SourceInfo = */ nullptr);

        JavascriptString *argString = JavascriptString::FromVar(evalArg);
        ScriptFunction *pfuncScript;
        wchar_t const * sourceString = argString->GetSz();
        charcount_t sourceLen = argString->GetLength();
        FastEvalMapString key(sourceString, sourceLen, moduleID, strictMode, isLibraryCode);
        if (!scriptContext->IsInEvalMap(key, isIndirect, &pfuncScript))
        {
            ulong grfscr = additionalGrfscr | fscrReturnExpression | fscrEval | fscrEvalCode | fscrGlobalCode;
            if (isLibraryCode)
            {
                grfscr |= fscrIsLibraryCode;
            }
            pfuncScript = library->GetGlobalObject()->EvalHelper(scriptContext, argString->GetSz(), argString->GetLength(), moduleID,
                grfscr, Constants::EvalCode, doRegisterDocument, isIndirect, strictMode);
            Assert(!pfuncScript->GetFunctionInfo()->IsGenerator());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            if (scriptContext->IsInDebugMode())
            {
                if (!(pfuncScript->GetFunctionBody()->GetUtf8SourceInfo()->GetIsLibraryCode() || pfuncScript->GetFunctionBody()->IsByteCodeDebugMode()))
                {
                    // Identifying if any function escaped for not being in debug mode. (This can be removed as a part of TFS : 935011)
                    Throw::FatalInternalError();
                }
            }
#endif
            scriptContext->AddToEvalMap(key, isIndirect, pfuncScript);
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        else
        {
            if (scriptContext->IsInDebugMode())
            {
                if (!(pfuncScript->GetFunctionBody()->GetUtf8SourceInfo()->GetIsLibraryCode() || pfuncScript->GetFunctionBody()->IsByteCodeDebugMode()))
                {
                    // Identifying if any function escaped for not being in debug mode. (This can be removed as a part of TFS : 935011)
                    Throw::FatalInternalError();
                }
            }
        }
#endif

        //We shouldn't be serializing eval functions; unless with -ForceSerialized flag
        if (CONFIG_FLAG(ForceSerialized)) {
            pfuncScript->GetFunctionProxy()->EnsureDeserialized();
        }
        if (pfuncScript->GetFunctionBody()->GetHasThis())
        {
            // The eval expression refers to "this"
            if (args.Info.Flags & CallFlags_ExtraArg)
            {
                JavascriptFunction* pfuncCaller;
                JavascriptStackWalker::GetCaller(&pfuncCaller, scriptContext);
                // If we are non-hidden call to eval then look for the "this" object in the frame display if the caller is a lambda else get "this" from the caller's frame.

                FunctionInfo* functionInfo = pfuncCaller->GetFunctionInfo();
                if (functionInfo != nullptr && (functionInfo->IsLambda() || functionInfo->IsClassConstructor()))
                {
                    Var defaultInstance = (moduleID == kmodGlobal) ? JavascriptOperators::OP_LdRoot(scriptContext)->ToThis() : (Var)JavascriptOperators::GetModuleRoot(moduleID, scriptContext);
                    varThis = JavascriptOperators::OP_GetThisScoped(environment, defaultInstance, scriptContext);
                    UpdateThisForEval(varThis, moduleID, scriptContext, strictMode);
                }
                else
                {
                    JavascriptStackWalker::GetThis(&varThis, moduleID, scriptContext);
                    UpdateThisForEval(varThis, moduleID, scriptContext, strictMode);
                }
            }
            else
            {
                // The expression, which refers to "this", is evaluated by an indirect eval.
                // Set "this" to the current module root.
                varThis = JavascriptOperators::OP_GetThis(scriptContext->GetLibrary()->GetUndefined(), moduleID, scriptContext);
            }
        }

        if (pfuncScript->HasSuperReference())
        {
            // Indirect evals cannot have a super reference.
            if (!(args.Info.Flags & CallFlags_ExtraArg))
            {
                JavascriptError::ThrowSyntaxError(scriptContext, ERRSuperInIndirectEval, L"super");
            }
        }

        return library->GetGlobalObject()->ExecuteEvalParsedFunction(pfuncScript, environment, varThis);
    }

    void GlobalObject::UpdateThisForEval(Var &varThis, ModuleID moduleID, ScriptContext *scriptContext, BOOL strictMode)
    {
        if (strictMode)
        {
            varThis = JavascriptOperators::OP_StrictGetThis(varThis, scriptContext);
        }
        else
        {
            varThis = JavascriptOperators::OP_GetThisNoFastPath(varThis, moduleID, scriptContext);
        }
    }


    Var GlobalObject::ExecuteEvalParsedFunction(ScriptFunction *pfuncScript, FrameDisplay* environment, Var &varThis)
    {
        Assert(pfuncScript != nullptr);

        pfuncScript->SetEnvironment(environment);
        //This function is supposed to be deserialized
        Assert(pfuncScript->GetFunctionBody());
        if (pfuncScript->GetFunctionBody()->GetFuncEscapes())
        {
            // Executing the eval causes the scope chain to escape.
            pfuncScript->InvalidateCachedScopeChain();
        }
        Var varResult = pfuncScript->GetEntryPoint()(pfuncScript, CallInfo(CallFlags_Eval, 1), varThis);
        pfuncScript->SetEnvironment(nullptr);
        return varResult;
    }

    ScriptFunction* GlobalObject::ProfileModeEvalHelper(ScriptContext* scriptContext, const wchar_t *source, int sourceLength, ModuleID moduleID, ulong grfscr, LPCOLESTR pszTitle, BOOL registerDocument, BOOL isIndirect, BOOL strictMode)
    {
#if ENABLE_DEBUG_CONFIG_OPTIONS
        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
        ScriptFunction *pEvalFunction = DefaultEvalHelper(scriptContext, source, sourceLength, moduleID, grfscr, pszTitle, registerDocument, isIndirect, strictMode);
        Assert(pEvalFunction);
        Js::FunctionProxy *proxy = pEvalFunction->GetFunctionProxy();
        Assert(proxy);

        OUTPUT_TRACE(Js::ScriptProfilerPhase, L"GlobalObject::ProfileModeEvalHelper FunctionNumber : %s, Entrypoint : 0x%08X IsFunctionDefer : %d\n",
                                    proxy->GetDebugNumberSet(debugStringBuffer), pEvalFunction->GetEntryPoint(), proxy->IsDeferred());

        if (proxy->IsDeferred())
        {
            // This could happen if the top level function is marked as deferred, we need to parse this to generate the script compile information (RegisterScript depends on that)
            Js::JavascriptFunction::DeferredParse(&pEvalFunction);
        }

        scriptContext->RegisterScript(proxy);

        return pEvalFunction;
    }

    void GlobalObject::ValidateSyntax(ScriptContext* scriptContext, const wchar_t *source, int sourceLength, bool isGenerator, void (Parser::*validateSyntax)())
    {
        Assert(sourceLength >= 0);

        HRESULT hr = S_OK;
        HRESULT hrParser = E_FAIL;
        CompileScriptException se;

        BEGIN_LEAVE_SCRIPT_INTERNAL(scriptContext);
        BEGIN_TRANSLATE_EXCEPTION_TO_HRESULT
        {
            ArenaAllocator tempAlloc(L"ValidateSyntaxArena", scriptContext->GetThreadContext()->GetPageAllocator(), Throw::OutOfMemory);

            size_t cchSource = sourceLength;
            size_t cbUtf8Buffer = (cchSource + 1) * 3;
            LPUTF8 utf8Source = AnewArray(&tempAlloc, utf8char_t, cbUtf8Buffer);
            Assert(cchSource < MAXLONG);
            size_t cbSource = utf8::EncodeIntoAndNullTerminate(utf8Source, source, static_cast< charcount_t >(cchSource));
            utf8Source = reinterpret_cast< LPUTF8 >( tempAlloc.Realloc(utf8Source, cbUtf8Buffer, cbSource + 1) );

            Parser parser(scriptContext);
            hrParser = parser.ValidateSyntax(utf8Source, cbSource, isGenerator, &se, validateSyntax);
        }
        END_TRANSLATE_EXCEPTION_TO_HRESULT(hr);
        END_LEAVE_SCRIPT_INTERNAL(scriptContext);

        if (FAILED(hr))
        {
            if (hr == E_OUTOFMEMORY)
            {
                JavascriptError::ThrowOutOfMemoryError(scriptContext);
            }
            if (hr == VBSERR_OutOfStack)
            {
                JavascriptError::ThrowStackOverflowError(scriptContext);
            }
        }
        if (!SUCCEEDED(hrParser))
        {
            hrParser = SCRIPT_E_RECORDED;
            EXCEPINFO ei;
            se.GetError(&hrParser, &ei);

            ErrorTypeEnum errorType;
            switch ((HRESULT)ei.scode)
            {
    #define RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) \
            case name: \
                errorType = jst; \
                break;
    #define RT_PUBLICERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource)
    #include "rterrors.h"
    #undef RT_PUBLICERROR_MSG
    #undef RT_ERROR_MSG
            default:
                errorType = kjstSyntaxError;
            }
            JavascriptError::MapAndThrowError(scriptContext, ei.scode, errorType, &ei);
        }
    }

    ScriptFunction* GlobalObject::DefaultEvalHelper(ScriptContext* scriptContext, const wchar_t *source, int sourceLength, ModuleID moduleID, ulong grfscr, LPCOLESTR pszTitle, BOOL registerDocument, BOOL isIndirect, BOOL strictMode)
    {
        Assert(sourceLength >= 0);
        AnalysisAssert(scriptContext);
        if (scriptContext->GetThreadContext()->EvalDisabled())
        {
            throw Js::EvalDisabledException();
        }

#ifdef PROFILE_EXEC
        scriptContext->ProfileBegin(Js::EvalCompilePhase);
#endif
        void * frameAddr = nullptr;
        GET_CURRENT_FRAME_ID(frameAddr);

        HRESULT hr = S_OK;
        HRESULT hrParser = S_OK;
        HRESULT hrCodeGen = S_OK;
        CompileScriptException se;
        Js::ParseableFunctionInfo * funcBody = NULL;

        BEGIN_LEAVE_SCRIPT_INTERNAL(scriptContext);
        BEGIN_TRANSLATE_EXCEPTION_TO_HRESULT
        {
            uint cchSource = sourceLength;
            size_t cbUtf8Buffer = (cchSource + 1) * 3;

            ArenaAllocator tempArena(L"EvalHelperArena", scriptContext->GetThreadContext()->GetPageAllocator(), Js::Throw::OutOfMemory);
            LPUTF8 utf8Source = AnewArray(&tempArena, utf8char_t, cbUtf8Buffer);

            Assert(cchSource < MAXLONG);
            size_t cbSource = utf8::EncodeIntoAndNullTerminate(utf8Source, source, static_cast< charcount_t >(cchSource));
            Assert(cbSource + 1 <= cbUtf8Buffer);

            SRCINFO const * pSrcInfo = scriptContext->GetModuleSrcInfo(moduleID);
            // Source Info objects are kept alive by the function bodies that are referencing it
            // The function body is created in GenerateByteCode but the source info isn't passed in, only the index
            // So we need to pin it here (TODO: Change GenerateByteCode to take in the sourceInfo itself)
            ENTER_PINNED_SCOPE(Utf8SourceInfo, sourceInfo);
            sourceInfo = Utf8SourceInfo::New(scriptContext, utf8Source, cchSource, cbSource, pSrcInfo);
            if ((grfscr & fscrIsLibraryCode) != 0)
            {
                sourceInfo->SetIsLibraryCode();
            }

            Parser parser(scriptContext, strictMode);
            bool forceNoNative = false;

            ParseNodePtr parseTree;

            SourceContextInfo * sourceContextInfo = pSrcInfo->sourceContextInfo;
            ULONG deferParseThreshold = Parser::GetDeferralThreshold(sourceContextInfo->IsSourceProfileLoaded());
            if ((ULONG)sourceLength > deferParseThreshold && !PHASE_OFF1(Phase::DeferParsePhase))
            {
                // Defer function bodies declared inside large dynamic blocks.
                grfscr |= fscrDeferFncParse;
            }

            grfscr = grfscr | fscrDynamicCode;

            hrParser = parser.ParseCesu8Source(&parseTree, utf8Source, cbSource, grfscr, &se, &sourceContextInfo->nextLocalFunctionId,
                sourceContextInfo);
            sourceInfo->SetParseFlags(grfscr);

            if (SUCCEEDED(hrParser) && parseTree)
            {
                // This keeps function bodies generated by the byte code alive till we return
                Js::AutoDynamicCodeReference dynamicFunctionReference(scriptContext);

                Assert(cchSource < MAXLONG);
                uint sourceIndex = scriptContext->SaveSourceNoCopy(sourceInfo, cchSource, true);

                // Tell byte code gen not to attempt to interact with the caller's context if this is indirect eval.
                // TODO: Handle strict mode.
                if (isIndirect &&
                    !strictMode &&
                    !parseTree->sxFnc.GetStrictMode())
                {
                    grfscr &= ~fscrEval;
                }
                hrCodeGen = GenerateByteCode(parseTree, grfscr, scriptContext, &funcBody, sourceIndex, forceNoNative, &parser, &se);
                sourceInfo->SetByteCodeGenerationFlags(grfscr);
            }

            LEAVE_PINNED_SCOPE();
        }
        END_TRANSLATE_EXCEPTION_TO_HRESULT(hr);
        END_LEAVE_SCRIPT_INTERNAL(scriptContext);


#ifdef PROFILE_EXEC
        scriptContext->ProfileEnd(Js::EvalCompilePhase);
#endif
        if (hr == E_OUTOFMEMORY)
        {
            JavascriptError::ThrowOutOfMemoryError(scriptContext);
        }
        else if(hr == VBSERR_OutOfStack)
        {
            JavascriptError::ThrowStackOverflowError(scriptContext);
        }
        else if(hr == E_ABORT)
        {
            throw Js::ScriptAbortException();
        }
        else if(FAILED(hr))
        {
            throw Js::InternalErrorException();
        }

        if (!SUCCEEDED(hrParser))
        {
            JavascriptError::ThrowParserError(scriptContext, hrParser, &se);
        }
        else if (!SUCCEEDED(hrCodeGen))
        {
            Assert(hrCodeGen == SCRIPT_E_RECORDED);
            hrCodeGen = se.ei.scode;
            /*
             * VBSERR_OutOfStack is of type kjstError but we throw a (more specific) StackOverflowError when a hard stack
             * overflow occurs. To keep the behavior consistent I'm special casing it here.
             */
            se.Free();
            if (hrCodeGen == VBSERR_OutOfStack)
            {
                JavascriptError::ThrowStackOverflowError(scriptContext);
            }
            JavascriptError::MapAndThrowError(scriptContext, hrCodeGen);
        }
        else
        {
            if (se.ei.scode == JSERR_AsmJsCompileError)
            {
                // if asm.js compilation succeeded, retry with asm.js disabled
                grfscr |= fscrNoAsmJs;
                se.Clear();
                return DefaultEvalHelper(scriptContext, source, sourceLength, moduleID, grfscr, pszTitle, registerDocument, isIndirect, strictMode);
            }

            Assert(funcBody != nullptr);
            funcBody->SetDisplayName(pszTitle);

            // Set the functionbody information to dynamic content PROFILER_SCRIPT_TYPE_DYNAMIC
            funcBody->SetIsTopLevel(true);

            // If not library code then let's find the parent, we may need to register this source if any exception happens later
            if ((grfscr & fscrIsLibraryCode) == 0)
            {
                // For parented eval get the caller's utf8SourceInfo
                JavascriptFunction* pfuncCaller;
                if (JavascriptStackWalker::GetCaller(&pfuncCaller, scriptContext) && pfuncCaller && pfuncCaller->IsScriptFunction())
                {
                    FunctionBody* parentFuncBody = pfuncCaller->GetFunctionBody();
                    Utf8SourceInfo* parentUtf8SourceInfo = parentFuncBody->GetUtf8SourceInfo();
                    Utf8SourceInfo* utf8SourceInfo = funcBody->GetFunctionProxy()->GetUtf8SourceInfo();
                    utf8SourceInfo->SetCallerUtf8SourceInfo(parentUtf8SourceInfo);
                }
            }

            if (registerDocument)
            {
                funcBody->RegisterFuncToDiag(scriptContext, pszTitle);
                funcBody = funcBody->GetParseableFunctionInfo(); // RegisterFunction may parse and update function body
            }

            ScriptFunction* pfuncScript = funcBody->IsGenerator() ?
                scriptContext->GetLibrary()->CreateGeneratorVirtualScriptFunction(funcBody) :
                scriptContext->GetLibrary()->CreateScriptFunction(funcBody);

            return pfuncScript;
        }
    }

#ifdef IR_VIEWER
    Var GlobalObject::IRDumpEvalHelper(ScriptContext* scriptContext, const wchar_t *source,
        int sourceLength, ModuleID moduleID, ulong grfscr, LPCOLESTR pszTitle,
        BOOL registerDocument, BOOL isIndirect, BOOL strictMode)
    {
        // TODO (t-doilij) clean up this function, specifically used for IR dump (don't execute bytecode; potentially dangerous)

        Assert(sourceLength >= 0);

        if (scriptContext->GetThreadContext()->EvalDisabled())
        {
            throw Js::EvalDisabledException();
        }

#ifdef PROFILE_EXEC
        scriptContext->ProfileBegin(Js::EvalCompilePhase);
#endif
        void * frameAddr = nullptr;
        GET_CURRENT_FRAME_ID(frameAddr);

        HRESULT hr = S_OK;
        HRESULT hrParser = S_OK;
        HRESULT hrCodeGen = S_OK;
        CompileScriptException se;
        Js::ParseableFunctionInfo * funcBody = NULL;

        BEGIN_LEAVE_SCRIPT_INTERNAL(scriptContext);
        BEGIN_TRANSLATE_EXCEPTION_TO_HRESULT
        {
            size_t cchSource = sourceLength;
            size_t cbUtf8Buffer = (cchSource + 1) * 3;

            ArenaAllocator tempArena(L"EvalHelperArena", scriptContext->GetThreadContext()->GetPageAllocator(), Js::Throw::OutOfMemory);
            LPUTF8 utf8Source = AnewArray(&tempArena, utf8char_t, cbUtf8Buffer);

            Assert(cchSource < MAXLONG);
            size_t cbSource = utf8::EncodeIntoAndNullTerminate(utf8Source, source, static_cast< charcount_t >(cchSource));
            Assert(cbSource + 1 <= cbUtf8Buffer);

            SRCINFO const * pSrcInfo = scriptContext->GetModuleSrcInfo(moduleID);
            // Source Info objects are kept alive by the function bodies that are referencing it
            // The function body is created in GenerateByteCode but the source info isn't passed in, only the index
            // So we need to pin it here (TODO: Change GenerateByteCode to take in the sourceInfo itself)
            ENTER_PINNED_SCOPE(Utf8SourceInfo, sourceInfo);
            sourceInfo = Utf8SourceInfo::New(scriptContext, utf8Source, cchSource, cbSource, pSrcInfo);
            if ((grfscr & fscrIsLibraryCode) != 0)
            {
                sourceInfo->SetIsLibraryCode();
            }

            Parser parser(scriptContext, strictMode);
            bool forceNoNative = false;

            ParseNodePtr parseTree;

            SourceContextInfo * sourceContextInfo = pSrcInfo->sourceContextInfo;

            grfscr = grfscr | fscrDynamicCode;

            hrParser = parser.ParseCesu8Source(&parseTree, utf8Source, cbSource, grfscr, &se, &sourceContextInfo->nextLocalFunctionId,
                sourceContextInfo);
            sourceInfo->SetParseFlags(grfscr);

            if (SUCCEEDED(hrParser) && parseTree)
            {
                // This keeps function bodies generated by the byte code alive till we return
                Js::AutoDynamicCodeReference dynamicFunctionReference(scriptContext);

                Assert(cchSource < MAXLONG);
                uint sourceIndex = scriptContext->SaveSourceNoCopy(sourceInfo, cchSource, true);

                grfscr = grfscr | fscrIrDumpEnable;

                hrCodeGen = GenerateByteCode(parseTree, grfscr, scriptContext, &funcBody, sourceIndex, forceNoNative, &parser, &se);
                sourceInfo->SetByteCodeGenerationFlags(grfscr);
            }

            LEAVE_PINNED_SCOPE();
        }
        END_TRANSLATE_EXCEPTION_TO_HRESULT(hr);
        END_LEAVE_SCRIPT_INTERNAL(scriptContext);


#ifdef PROFILE_EXEC
        scriptContext->ProfileEnd(Js::EvalCompilePhase);
#endif
        if (hr == E_OUTOFMEMORY)
        {
            JavascriptError::ThrowOutOfMemoryError(scriptContext);
        }
        else if(hr == VBSERR_OutOfStack)
        {
            JavascriptError::ThrowStackOverflowError(scriptContext);
        }
        else if(hr == E_ABORT)
        {
            throw Js::ScriptAbortException();
        }
        else if(FAILED(hr))
        {
            throw Js::InternalErrorException();
        }

        if (!SUCCEEDED(hrParser))
        {
            hrParser = SCRIPT_E_RECORDED;
            EXCEPINFO ei;
            se.GetError(&hrParser, &ei);

            ErrorTypeEnum errorType;
            switch ((HRESULT)ei.scode)
            {
    #define RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) \
            case name: \
                errorType = jst; \
                break;
    #define RT_PUBLICERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource)
    #include "rterrors.h"
    #undef RT_PUBLICERROR_MSG
    #undef RT_ERROR_MSG
            default:
                errorType = kjstSyntaxError;
            }
            JavascriptError::MapAndThrowError(scriptContext, ei.scode, errorType, &ei);
        }
        else if (!SUCCEEDED(hrCodeGen))
        {
            Assert(hrCodeGen == SCRIPT_E_RECORDED);
            hrCodeGen = se.ei.scode;
            /*
             * VBSERR_OutOfStack is of type kjstError but we throw a (more specific) StackOverflowError when a hard stack
             * overflow occurs. To keep the behavior consistent I'm special casing it here.
             */
            se.Free();
            if (hrCodeGen == VBSERR_OutOfStack)
            {
                JavascriptError::ThrowStackOverflowError(scriptContext);
            }
            JavascriptError::MapAndThrowError(scriptContext, hrCodeGen);
        }
        else
        {
            Assert(funcBody != nullptr);
            funcBody->SetDisplayName(pszTitle);

            // Set the functionbody information to dynamic content PROFILER_SCRIPT_TYPE_DYNAMIC
            funcBody->SetIsTopLevel(true);

            if (registerDocument)
            {
                funcBody->RegisterFuncToDiag(scriptContext, pszTitle);
                funcBody = funcBody->GetParseableFunctionInfo(); // RegisterFunction may parse and update function body
            }

            NativeCodeGenerator *nativeCodeGenerator = scriptContext->GetNativeCodeGenerator();
            Js::FunctionBody *functionBody = funcBody->GetFunctionBody(); // funcBody is ParseableFunctionInfo

            if (!functionBody || !functionBody->GetByteCode())
            {
                return scriptContext->GetLibrary()->GetUndefined();
            }

            Js::Var retVal = RejitIRViewerFunction(nativeCodeGenerator, functionBody, scriptContext);
            return retVal;
        }
    }
#endif /* IR_VIEWER */

    Var GlobalObject::EntryParseInt(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        JavascriptString *str;
        int radix = 0;


        if(args.Info.Count < 2)
        {
            // We're really converting "undefined" here, but "undefined" produces
            // NaN, so just return that directly.
            return JavascriptNumber::ToVarNoCheck(JavascriptNumber::NaN, scriptContext);
        }


        // Short cut for numbers and radix 10
        if (((args.Info.Count == 2) ||
            (TaggedInt::Is(args[2]) && TaggedInt::ToInt32(args[2]) == 10) ||
            (JavascriptNumber::Is(args[2]) && JavascriptNumber::GetValue(args[2]) == 10)))
        {
            if (TaggedInt::Is(args[1]))
            {
                return args[1];
            }
            if (JavascriptNumber::Is(args[1]))
            {
                double value = JavascriptNumber::GetValue(args[1]);

                // make sure we are in the ranges that don't have exponential notation.
                double absValue = Math::Abs(value);
                if (absValue < 1.0e21 && absValue >= 1e-5)
                {
                    double result;
#pragma prefast(suppress:6031, "We don't care about the fraction part")
                    ::modf(value, &result);
                    return JavascriptNumber::ToVarIntCheck(result, scriptContext);
                }
            }
        }

        // convert input to a string
        if (JavascriptString::Is(args[1]))
        {
            str = JavascriptString::FromVar(args[1]);
        }
        else
        {
            str = JavascriptConversion::ToString(args[1], scriptContext);
        }

        if(args.Info.Count > 2)
        {
            // retrieve the radix
            // TODO : verify for ES5 : radix is 10 when undefined or 0, except when it starts with 0x when it is 16.
            radix = JavascriptConversion::ToInt32(args[2], scriptContext);
            if(radix < 0 || radix == 1 || radix > 36)
            {
                //illegal, return NaN
                return JavascriptNumber::ToVarNoCheck(JavascriptNumber::NaN, scriptContext);
            }
        }

        Var result = str->ToInteger(radix);
        return result;
    }

    Var GlobalObject::EntryParseFloat(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        JavascriptString *str;

        if(args.Info.Count < 2)
        {
            // We're really converting "undefined" here, but "undefined" produces
            // NaN, so just return that directly.
            return JavascriptNumber::ToVarNoCheck(JavascriptNumber::NaN, scriptContext);
        }

        // Short cut for numbers
        if (TaggedInt::Is(args[1]))
        {
            return args[1];
        }
        if(JavascriptNumber::Is_NoTaggedIntCheck(args[1]))
        {
            // If the value is negative zero, return positive zero. Since the parameter type is string, -0 would first be
            // converted to the string "0", then parsed to result in positive zero.
            return
                JavascriptNumber::IsNegZero(JavascriptNumber::GetValue(args[1])) ?
                    TaggedInt::ToVarUnchecked(0) :
                    args[1];
        }

        // convert input to a string
        if (JavascriptString::Is(args[1]))
        {
            str = JavascriptString::FromVar(args[1]);
        }
        else
        {
            str = JavascriptConversion::ToString(args[1], scriptContext);
        }

        // skip preceding whitespace
        const wchar_t *pch = scriptContext->GetCharClassifier()->SkipWhiteSpace(str->GetSz());

        // perform the string -> float conversion
        double result = NumberUtilities::StrToDbl(pch, &pch, scriptContext);

        return JavascriptNumber::ToVarNoCheck(result, scriptContext);
    }

    Var GlobalObject::EntryIsNaN(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        if(args.Info.Count < 2)
        {
            return scriptContext->GetLibrary()->GetTrue();
        }
        return JavascriptBoolean::ToVar(
            JavascriptNumber::IsNan(JavascriptConversion::ToNumber(args[1],scriptContext)),
            scriptContext);
    }

    Var GlobalObject::EntryIsFinite(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if(args.Info.Count < 2)
        {
            return scriptContext->GetLibrary()->GetFalse();
        }
        return JavascriptBoolean::ToVar(
            NumberUtilities::IsFinite(JavascriptConversion::ToNumber(args[1],scriptContext)),
            scriptContext);
    }

    Var GlobalObject::EntryDecodeURI(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        return UriHelper::DecodeCoreURI(scriptContext, args, UriHelper::URIReserved);
    }

    Var GlobalObject::EntryDecodeURIComponent(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        return UriHelper::DecodeCoreURI(scriptContext, args, UriHelper::URINone);
    }

    Var GlobalObject::EntryEncodeURI(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        return UriHelper::EncodeCoreURI(scriptContext, args, UriHelper::URIReserved | UriHelper::URIUnescaped);
    }

    Var GlobalObject::EntryEncodeURIComponent(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        return UriHelper::EncodeCoreURI(scriptContext, args, UriHelper::URIUnescaped);
    }

    //
    // Part of Appendix B of ES5 spec
    //

    // This is a bit field for the hex values: 00-29, 2C, 3A-3F, 5B-5E, 60, 7B-FF
    // These are the values escape encodes using the default mask (or mask >= 4)
    static const BYTE _grfbitEscape[] =
    {
        0xFF, 0xFF, // 00 - 0F
        0xFF, 0xFF, // 10 - 1F
        0xFF, 0x13, // 20 - 2F
        0x00, 0xFC, // 30 - 3F
        0x00, 0x00, // 40 - 4F
        0x00, 0x78, // 50 - 5F
        0x01, 0x00, // 60 - 6F
        0x00, 0xF8, // 70 - 7F
        0xFF, 0xFF, // 80 - 8F
        0xFF, 0xFF, // 90 - 9F
        0xFF, 0xFF, // A0 - AF
        0xFF, 0xFF, // B0 - BF
        0xFF, 0xFF, // C0 - CF
        0xFF, 0xFF, // D0 - DF
        0xFF, 0xFF, // E0 - EF
        0xFF, 0xFF, // F0 - FF
    };
    Var GlobalObject::EntryEscape(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count <= 1)
        {
            return scriptContext->GetLibrary()->GetUndefinedDisplayString();
        }

        JavascriptString *src = JavascriptConversion::ToString(args[1], scriptContext);

        CompoundString *const bs = CompoundString::NewWithCharCapacity(src->GetLength(), scriptContext->GetLibrary());
        wchar_t chw;
        wchar_t * pchSrc;
        wchar_t * pchLim;

        const char _rgchHex[] = "0123456789ABCDEF";

        pchSrc = const_cast<wchar_t *>(src->GetString());
        pchLim = pchSrc + src->GetLength();
        while (pchSrc < pchLim)
        {
            chw = *pchSrc++;

            if (0 != (chw & 0xFF00))
            {
                bs->AppendChars(L"%u");
                bs->AppendChars(static_cast<wchar_t>(_rgchHex[(chw >> 12) & 0x0F]));
                bs->AppendChars(static_cast<wchar_t>(_rgchHex[(chw >> 8) & 0x0F]));
                bs->AppendChars(static_cast<wchar_t>(_rgchHex[(chw >> 4) & 0x0F]));
                bs->AppendChars(static_cast<wchar_t>(_rgchHex[chw & 0x0F]));
            }
            else if (_grfbitEscape[chw >> 3] & (1 << (chw & 7)))
            {
                // Escape the byte.
                bs->AppendChars(L'%');
                bs->AppendChars(static_cast<wchar_t>(_rgchHex[chw >> 4]));
                bs->AppendChars(static_cast<wchar_t>(_rgchHex[chw & 0x0F]));
            }
            else
            {
                bs->AppendChars(chw);
            }
        }

        return bs;
    }

    //
    // Part of Appendix B of ES5 spec
    //
    Var GlobalObject::EntryUnEscape(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count <= 1)
        {
            return scriptContext->GetLibrary()->GetUndefinedDisplayString();
        }

        wchar_t chw;
        wchar_t chT;
        wchar_t * pchSrc;
        wchar_t * pchLim;
        wchar_t * pchMin;

        JavascriptString *src = JavascriptConversion::ToString(args[1], scriptContext);

        CompoundString *const bs = CompoundString::NewWithCharCapacity(src->GetLength(), scriptContext->GetLibrary());

        pchSrc = const_cast<wchar_t *>(src->GetString());
        pchLim = pchSrc + src->GetLength();
        while (pchSrc < pchLim)
        {
            if ('%' != (chT = *pchSrc++))
            {
                bs->AppendChars(chT);
                continue;
            }

            pchMin = pchSrc;
            chT = *pchSrc++;
            if ('u' != chT)
            {
                chw = 0;
                goto LTwoHexDigits;
            }

            // 4 hex digits.
            if ((chT = *pchSrc++ - '0') <= 9)
                chw = chT * 0x1000;
            else if ((chT -= 'A' - '0') <= 5 || (chT -= 'a' - 'A') <= 5)
                chw = (chT + 10) * 0x1000;
            else
                goto LHexError;
            if ((chT = *pchSrc++ - '0') <= 9)
                chw += chT * 0x0100;
            else if ((chT -= 'A' - '0') <= 5 || (chT -= 'a' - 'A') <= 5)
                chw += (chT + 10) * 0x0100;
            else
                goto LHexError;
            chT = *pchSrc++;
LTwoHexDigits:
            if ((chT -= '0') <= 9)
                chw += chT * 0x0010;
            else if ((chT -= 'A' - '0') <= 5 || (chT -= 'a' - 'A') <= 5)
                chw += (chT + 10) * 0x0010;
            else
                goto LHexError;
            if ((chT = *pchSrc++ - '0') <= 9)
                chw += chT;
            else if ((chT -= 'A' - '0') <= 5 || (chT -= 'a' - 'A') <= 5)
                chw += chT + 10;
            else
            {
LHexError:
                pchSrc = pchMin;
                chw = '%';
            }

            bs->AppendChars(chw);
        }

        return bs;
    }

#if DBG
    void DebugClearStack()
    {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
    }
#endif

    Var GlobalObject::EntryCollectGarbage(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

#if DBG_DUMP
        if (Js::Configuration::Global.flags.TraceWin8Allocations)
        {
            Output::Print(L"MemoryTrace: GlobalObject::EntryCollectGarbage Invoke\n");
            Output::Flush();
        }
#endif


#if DBG
        // Clear 1K of stack to avoid false positive in debug build.
        // Because we don't debug build don't stack pack
        DebugClearStack();
#endif
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();

        if (!scriptContext->GetConfig()->IsCollectGarbageEnabled())
        {
            //We expose the CollectGarbage API with flag for compat reasons. Though we don't trigger GC if CollectGarbage key is not present.
            return scriptContext->GetLibrary()->GetUndefined();
        }

        Recycler* recycler = scriptContext->GetRecycler();
        if (recycler)
        {
#if DBG && defined(RECYCLER_DUMP_OBJECT_GRAPH)
            ARGUMENTS(args, callInfo);
            if (args.Info.Count > 1)
            {
                double value = JavascriptConversion::ToNumber(args[1], function->GetScriptContext());
                if (value == 1.0)
                {
                    recycler->dumpObjectOnceOnCollect = true;
                }
            }
#endif
            recycler->CollectNow<CollectNowDecommitNowExplicit>();
        }

#if DBG_DUMP
#ifdef ENABLE_PROJECTION
        scriptContext->GetThreadContext()->DumpProjectionContextMemoryStats(L"Stats after GlobalObject::EntryCollectGarbage call");
#endif

        if (Js::Configuration::Global.flags.TraceWin8Allocations)
        {
            Output::Print(L"MemoryTrace: GlobalObject::EntryCollectGarbage Exit\n");
            Output::Flush();
        }
#endif

        return scriptContext->GetLibrary()->GetUndefined();
    }

    //Pattern match is unique to RuntimeObject. Only leading and trailing * are implemented
    //Example: *pat*tern* actually matches all the strings having pat*tern as substring.
    BOOL GlobalObject::MatchPatternHelper(JavascriptString *propertyName, JavascriptString *pattern, ScriptContext *scriptContext)
    {
        if (nullptr == propertyName || nullptr == pattern)
        {
            return FALSE;
        }

        charcount_t     patternLength = pattern->GetLength();
        charcount_t     nameLength = propertyName->GetLength();
        BOOL    bStart;
        BOOL    bEnd;

        if (0 == patternLength)
        {
            return TRUE; // empty pattern matches all
        }

        bStart = (L'*' == pattern->GetItem(0));// Is there a start star?
        bEnd = (L'*' == pattern->GetItem(patternLength - 1));// Is there an end star?

        //deal with the stars
        if (bStart || bEnd)
        {
            JavascriptString *subPattern;
            int idxStart = bStart? 1: 0;
            int idxEnd   = bEnd? (patternLength - 1): patternLength;
            if (idxEnd <= idxStart)
            {
                return TRUE; //scenario double star **
            }

            //get a pattern which doesn't contain leading and trailing stars
            subPattern = JavascriptString::FromVar(JavascriptString::SubstringCore(pattern, idxStart, idxEnd - idxStart, scriptContext));

            uint index = JavascriptString::strstr(propertyName, subPattern, false);

            if (index == (uint)-1)
            {
                return FALSE;
            }
            if (FALSE == bStart)
            {
                return index == 0; //begin at the same place;
            }
            else if (FALSE == bEnd)
            {
                return (index + patternLength - 1) == (nameLength);
            }
            return TRUE;
        }
        else
        {
            //full match
            if (0 == JavascriptString::strcmp(propertyName, pattern))
            {
                return TRUE;
            }
        }

        return FALSE;
    }

    BOOL GlobalObject::HasProperty(PropertyId propertyId)
    {
        return DynamicObject::HasProperty(propertyId) ||
            (this->directHostObject && JavascriptOperators::HasProperty(this->directHostObject, propertyId)) ||
            (this->hostObject && JavascriptOperators::HasProperty(this->hostObject, propertyId));
    }

    BOOL GlobalObject::HasRootProperty(PropertyId propertyId)
    {
        return __super::HasRootProperty(propertyId) ||
            (this->directHostObject && JavascriptOperators::HasProperty(this->directHostObject, propertyId)) ||
            (this->hostObject && JavascriptOperators::HasProperty(this->hostObject, propertyId));
    }

    BOOL GlobalObject::HasOwnProperty(PropertyId propertyId)
    {
        return DynamicObject::HasProperty(propertyId) ||
            (this->directHostObject && this->directHostObject->HasProperty(propertyId));
    }

    BOOL GlobalObject::HasOwnPropertyNoHostObject(PropertyId propertyId)
    {
        return DynamicObject::HasProperty(propertyId);
    }

    BOOL GlobalObject::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        if (DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext))
        {
            return TRUE;
        }
        return (this->directHostObject && JavascriptOperators::GetProperty(this->directHostObject, propertyId, value, requestContext, info)) ||
            (this->hostObject && JavascriptOperators::GetProperty(this->hostObject, propertyId, value, requestContext, info));
    }

    BOOL GlobalObject::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return GlobalObject::GetProperty(originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext);
    }

    BOOL GlobalObject::GetRootProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        if (__super::GetRootProperty(originalInstance, propertyId, value, info, requestContext))
        {
            return TRUE;
        }
        return (this->directHostObject && JavascriptOperators::GetProperty(this->directHostObject, propertyId, value, requestContext, info)) ||
            (this->hostObject && JavascriptOperators::GetProperty(this->hostObject, propertyId, value, requestContext, info));
    }

    BOOL GlobalObject::GetAccessors(PropertyId propertyId, Var* getter, Var* setter, ScriptContext * requestContext)
    {
        if (DynamicObject::GetAccessors(propertyId, getter, setter, requestContext))
        {
            return TRUE;
        }
        if (this->directHostObject)
        {
            return this->directHostObject->GetAccessors(propertyId, getter, setter, requestContext);
        }
        else if (this->hostObject)
        {
            return this->hostObject->GetAccessors(propertyId, getter, setter, requestContext);
        }
        return FALSE;
    }

    BOOL GlobalObject::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info,
        ScriptContext* requestContext)
    {
        if (DynamicObject::GetPropertyReference(originalInstance, propertyId, value, info, requestContext))
        {
            return true;
        }
        return (this->directHostObject && JavascriptOperators::GetPropertyReference(this->directHostObject, propertyId, value, requestContext, info)) ||
            (this->hostObject && JavascriptOperators::GetPropertyReference(this->hostObject, propertyId, value, requestContext, info));
    }

    BOOL GlobalObject::GetRootPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info,
        ScriptContext* requestContext)
    {
        if (__super::GetRootPropertyReference(originalInstance, propertyId, value, info, requestContext))
        {
            return true;
        }
        return (this->directHostObject && JavascriptOperators::GetPropertyReference(this->directHostObject, propertyId, value, requestContext, info)) ||
            (this->hostObject && JavascriptOperators::GetPropertyReference(this->hostObject, propertyId, value, requestContext, info));
    }

    BOOL GlobalObject::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        // var x = 10; variables declared with "var" at global scope
        // These cannot be deleted
        // In ES5 they are enumerable.

        PropertyAttributes attributes = PropertyWritable | PropertyEnumerable | PropertyDeclaredGlobal;
        flags = static_cast<PropertyOperationFlags>(flags | PropertyOperation_ThrowIfNotExtensible);
        return DynamicObject::SetPropertyWithAttributes(propertyId, value, attributes, info, flags);
    }

    BOOL GlobalObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {
        // var x = 10; variables declared with "var" inside "eval"
        // These CAN be deleted
        // in ES5 they are enumerable.
        //
        PropertyAttributes attributes = PropertyDynamicTypeDefaults | PropertyDeclaredGlobal;
        return DynamicObject::SetPropertyWithAttributes(propertyId, value, attributes, NULL, PropertyOperation_ThrowIfNotExtensible);
    }

    BOOL GlobalObject::InitFuncScoped(PropertyId propertyId, Var value)
    {
        // Var binding of functions declared in eval are elided when conflicting
        // with global scope let/const variables, so do not actually set the
        // property if it exists and is a let/const variable.
        bool noRedecl = false;
        if (!GetTypeHandler()->HasRootProperty(this, propertyId, &noRedecl) || !noRedecl)
        {
            //
            // var x = 10; variables declared with "var" inside "eval"
            // These CAN be deleted
            // in ES5 they are enumerable.

            PropertyAttributes attributes = PropertyDynamicTypeDefaults;

            DynamicObject::SetPropertyWithAttributes(propertyId, value, attributes, NULL, PropertyOperation_ThrowIfNotExtensible);
        }
        return true;
    }

    BOOL GlobalObject::SetExistingProperty(PropertyId propertyId, Var value, PropertyValueInfo* info, BOOL *setAttempted)
    {
        BOOL hasOwnProperty = DynamicObject::HasProperty(propertyId);
        BOOL hasProperty = JavascriptOperators::HasProperty(this->GetPrototype(), propertyId);
        *setAttempted = TRUE;

        if (this->directHostObject &&
            !hasOwnProperty &&
            !hasProperty &&
            this->directHostObject->HasProperty(propertyId))
        {
            // directHostObject->HasProperty returns true for things in global collections, however, they are not able to be set.
            // When we call SetProperty we'll return FALSE which means fall back to GlobalObject::SetProperty and shadow our collection
            // object rather than failing the set altogether. See bug Windows Out Of Band Releases 1144780 and linked bugs for details.
            if (this->directHostObject->SetProperty(propertyId, value, PropertyOperation_None, info))
            {
                return TRUE;
            }
        }
        else
            if (this->hostObject &&
                // Consider to revert to the commented line and ignore the prototype chain when direct host is on by default in IE9 mode
                !hasOwnProperty &&
                !hasProperty &&
                this->hostObject->HasProperty(propertyId))
            {
                return this->hostObject->SetProperty(propertyId, value, PropertyOperation_None, NULL);
            }

        if (hasOwnProperty || hasProperty)
        {
            return DynamicObject::SetProperty(propertyId, value, PropertyOperation_None, info);
        }

        *setAttempted = FALSE;
        return FALSE;
    }

    BOOL GlobalObject::SetExistingRootProperty(PropertyId propertyId, Var value, PropertyValueInfo* info, BOOL *setAttempted)
    {
        BOOL hasOwnProperty = __super::HasRootProperty(propertyId);
        BOOL hasProperty = JavascriptOperators::HasProperty(this->GetPrototype(), propertyId);
        *setAttempted = TRUE;

        if (this->directHostObject &&
            !hasOwnProperty &&
            !hasProperty &&
            this->directHostObject->HasProperty(propertyId))
        {
            // directHostObject->HasProperty returns true for things in global collections, however, they are not able to be set.
            // When we call SetProperty we'll return FALSE which means fall back to GlobalObject::SetProperty and shadow our collection
            // object rather than failing the set altogether. See bug Windows Out Of Band Releases 1144780 and linked bugs for details.
            if (this->directHostObject->SetProperty(propertyId, value, PropertyOperation_None, info))
            {
                return TRUE;
            }
        }
        else
            if (this->hostObject &&
                // Consider to revert to the commented line and ignore the prototype chain when direct host is on by default in IE9 mode
                !hasOwnProperty &&
                !hasProperty &&
                this->hostObject->HasProperty(propertyId))
            {
                return this->hostObject->SetProperty(propertyId, value, PropertyOperation_None, NULL);
            }

        if (hasOwnProperty || hasProperty)
        {
            return __super::SetRootProperty(propertyId, value, PropertyOperation_None, info);
        }

        *setAttempted = FALSE;
        return FALSE;
    }

    BOOL GlobalObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        Assert(!(flags & PropertyOperation_Root));

        BOOL setAttempted = TRUE;
        if (this->SetExistingProperty(propertyId, value, info, &setAttempted))
        {
            return TRUE;
        }

        //
        // Set was attempted. But the set operation returned false.
        // This happens, when the property is read only.
        // In those scenarios, we should be setting the property with default attributes
        //
        if (setAttempted)
        {
            return FALSE;
        }

        // Windows 8 430092: When we set a new property on globalObject there may be stale inline caches holding onto directHostObject->prototype
        // properties of the same name. So we need to clear proto caches in this scenario. But check for same property in directHostObject->prototype
        // chain is expensive (call to DOM) compared to just invalidating the cache.
        // If this blind invalidation is expensive in any scenario then we need to revisit this.
        // Another solution proposed was not to cache any of the properties of window->directHostObject->prototype.
        // if ((this->directHostObject && JavascriptOperators::HasProperty(this->directHostObject->GetPrototype(), propertyId)) ||
        //    (this->hostObject && JavascriptOperators::HasProperty(this->hostObject->GetPrototype(), propertyId)))

        this->GetScriptContext()->InvalidateProtoCaches(propertyId);

        //
        // x = 10; implicit/phantom variable at global scope
        // These CAN be deleted
        // In ES5 they are enumerable.

        PropertyAttributes attributes = PropertyDynamicTypeDefaults;
        return DynamicObject::SetPropertyWithAttributes(propertyId, value, attributes, info);
    }

    BOOL GlobalObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        PropertyRecord const * propertyRecord;
        this->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return GlobalObject::SetProperty(propertyRecord->GetPropertyId(), value, flags, info);
    }

    BOOL GlobalObject::SetRootProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        Assert(flags & PropertyOperation_Root);

        BOOL setAttempted = TRUE;
        if (this->SetExistingRootProperty(propertyId, value, info, &setAttempted))
        {
            return TRUE;
        }

        //
        // Set was attempted. But the set operation returned false.
        // This happens, when the property is read only.
        // In those scenarios, we should be setting the property with default attributes
        //
        if (setAttempted)
        {
            return FALSE;
        }

        if (flags & PropertyOperation_StrictMode)
        {
            if (!GetScriptContext()->GetThreadContext()->RecordImplicitException())
            {
                return FALSE;
            }
            JavascriptError::ThrowReferenceError(GetScriptContext(), JSERR_RefErrorUndefVariable);
        }

        // Windows 8 430092: When we set a new property on globalObject there may be stale inline caches holding onto directHostObject->prototype
        // properties of the same name. So we need to clear proto caches in this scenario. But check for same property in directHostObject->prototype
        // chain is expensive (call to DOM) compared to just invalidating the cache.
        // If this blind invalidation is expensive in any scenario then we need to revisit this.
        // Another solution proposed was not to cache any of the properties of window->directHostObject->prototype.
        // if ((this->directHostObject && JavascriptOperators::HasProperty(this->directHostObject->GetPrototype(), propertyId)) ||
        //    (this->hostObject && JavascriptOperators::HasProperty(this->hostObject->GetPrototype(), propertyId)))

        this->GetScriptContext()->InvalidateProtoCaches(propertyId);

        return __super::SetRootProperty(propertyId, value, flags, info);
    }

    BOOL GlobalObject::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {
        if (DynamicObject::SetAccessors(propertyId, getter, setter, flags))
        {
            return TRUE;
        }
        if (this->directHostObject)
        {
            return this->directHostObject->SetAccessors(propertyId, getter, setter, flags);
        }
        if (this->hostObject)
        {
            return this->hostObject->SetAccessors(propertyId, getter, setter, flags);
        }
        return TRUE;
    }

    DescriptorFlags GlobalObject::GetSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        DescriptorFlags flags = DynamicObject::GetSetter(propertyId, setterValue, info, requestContext);
        if (flags == None)
        {
            if (this->directHostObject)
            {
                // We need to look up the prototype chain here.
                JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(this->directHostObject, propertyId, setterValue, &flags, info, requestContext);
            }
            else if (this->hostObject)
            {
                JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(this->hostObject, propertyId, setterValue, &flags, info, requestContext);
            }
        }

        return flags;
    }

    DescriptorFlags GlobalObject::GetSetter(JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return GlobalObject::GetSetter(propertyRecord->GetPropertyId(), setterValue, info, requestContext);
    }

    DescriptorFlags GlobalObject::GetRootSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        DescriptorFlags flags = __super::GetRootSetter(propertyId, setterValue, info, requestContext);
        if (flags == None)
        {
            if (this->directHostObject)
            {
                // We need to look up the prototype chain here.
                JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(this->directHostObject, propertyId, setterValue, &flags, info, requestContext);
            }
            else if (this->hostObject)
            {
                JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(this->hostObject, propertyId, setterValue, &flags, info, requestContext);
            }
        }

        return flags;
    }

    DescriptorFlags GlobalObject::GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {
        DescriptorFlags flags = DynamicObject::GetItemSetter(index, setterValue, requestContext);
        if (flags == None)
        {
            if (this->directHostObject)
            {
                // We need to look up the prototype chain here.
                JavascriptOperators::CheckPrototypesForAccessorOrNonWritableItem(this->directHostObject, index, setterValue, &flags, requestContext);
            }
            else if (this->hostObject)
            {
                JavascriptOperators::CheckPrototypesForAccessorOrNonWritableItem(this->hostObject, index, setterValue, &flags, requestContext);
            }
        }

        return flags;
    }

    BOOL GlobalObject::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {
        if (__super::HasProperty(propertyId))
        {
            return __super::DeleteProperty(propertyId, flags);
        }
        else if (this->directHostObject && this->directHostObject->HasProperty(propertyId))
        {
            return this->directHostObject->DeleteProperty(propertyId, flags);
        }
        else if (this->hostObject && this->hostObject->HasProperty(propertyId))
        {
            return this->hostObject->DeleteProperty(propertyId, flags);
        }

        // Non-existent property
        return TRUE;
    }

    BOOL GlobalObject::DeleteRootProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {
        if (__super::HasRootProperty(propertyId))
        {
            return __super::DeleteRootProperty(propertyId, flags);
        }
        else if (this->directHostObject && this->directHostObject->HasProperty(propertyId))
        {
            return this->directHostObject->DeleteProperty(propertyId, flags);
        }
        else if (this->hostObject && this->hostObject->HasProperty(propertyId))
        {
            return this->hostObject->DeleteProperty(propertyId, flags);
        }

        // Non-existent property
        return TRUE;
    }

    BOOL GlobalObject::HasItem(uint32 index)
    {
        return DynamicObject::HasItem(index)
            || (this->directHostObject && JavascriptOperators::HasItem(this->directHostObject, index))
            || (this->hostObject && JavascriptOperators::HasItem(this->hostObject, index));
    }

    BOOL GlobalObject::HasOwnItem(uint32 index)
    {
        return DynamicObject::HasItem(index)
            || (this->directHostObject && this->directHostObject->HasItem(index));
    }

    BOOL GlobalObject::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {
        if (DynamicObject::GetItemReference(originalInstance, index, value, requestContext))
        {
            return TRUE;
        }
        return (this->directHostObject && this->directHostObject->GetItemReference(originalInstance, index, value, requestContext)) ||
            (this->hostObject && this->hostObject->GetItemReference(originalInstance, index, value, requestContext));
    }

    BOOL GlobalObject::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {
        BOOL result = DynamicObject::SetItem(index, value, flags);
        return result;
    }

    BOOL GlobalObject::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {
        if (DynamicObject::GetItem(originalInstance, index, value, requestContext))
        {
            return TRUE;
        }

        return (this->directHostObject && this->directHostObject->GetItem(originalInstance, index, value, requestContext)) ||
            (this->hostObject && this->hostObject->GetItem(originalInstance, index, value, requestContext));
    }

    BOOL GlobalObject::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {
        if (DynamicObject::DeleteItem(index, flags))
        {
            return TRUE;
        }

        if (this->directHostObject)
        {
            return this->directHostObject->DeleteItem(index, flags);
        }

        if (this->hostObject)
        {
            return this->hostObject->DeleteItem(index, flags);
        }
        return FALSE;
    }

    BOOL GlobalObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"{...}");
        return TRUE;
    }

    BOOL GlobalObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"Object, (Global)");
        return TRUE;
    }

    BOOL GlobalObject::StrictEquals(Js::Var other, BOOL* value, ScriptContext * requestContext)
    {
        if (this == other)
        {
            *value = true;
            return TRUE;
        }
        else if (this->directHostObject)
        {
            return this->directHostObject->StrictEquals(other, value, requestContext);
        }
        else if (this->hostObject)
        {
            return this->hostObject->StrictEquals(other, value, requestContext);
        }
        return FALSE;
    }

    BOOL GlobalObject::Equals(Js::Var other, BOOL* value, ScriptContext * requestContext)
    {
        if (this == other)
        {
            *value = true;
            return TRUE;
        }
        else if (this->directHostObject)
        {
            return this->directHostObject->Equals(other, value, requestContext);
        }
        else if (this->hostObject)
        {
            return this->hostObject->Equals(other, value, requestContext);
        }

        *value = false;
        return TRUE;
    }
}

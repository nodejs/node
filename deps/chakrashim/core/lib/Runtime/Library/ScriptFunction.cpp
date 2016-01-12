//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    const wchar_t ScriptFunction::diagDefaultCtor[]         = JS_DEFAULT_CTOR_DISPLAY_STRING;
    const wchar_t ScriptFunction::diagDefaultExtendsCtor[]  = JS_DEFAULT_EXTENDS_CTOR_DISPLAY_STRING;

    ScriptFunctionBase::ScriptFunctionBase(DynamicType * type) :
        JavascriptFunction(type)
    {}

    ScriptFunctionBase::ScriptFunctionBase(DynamicType * type, FunctionInfo * functionInfo) :
        JavascriptFunction(type, functionInfo)
    {}

    bool ScriptFunctionBase::Is(Var func)
    {
        return ScriptFunction::Is(func) || JavascriptGeneratorFunction::Is(func);
    }

    ScriptFunctionBase * ScriptFunctionBase::FromVar(Var func)
    {
        Assert(ScriptFunctionBase::Is(func));
        return reinterpret_cast<ScriptFunctionBase *>(func);
    }

    ScriptFunction::ScriptFunction(DynamicType * type) :
        ScriptFunctionBase(type), environment((FrameDisplay*)&NullFrameDisplay),
        cachedScopeObj(nullptr), hasInlineCaches(false), hasSuperReference(false),
        isDefaultConstructor(false), isActiveScript(false)
    {}

    ScriptFunction::ScriptFunction(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType)
        : ScriptFunctionBase(deferredPrototypeType, proxy),
        environment((FrameDisplay*)&NullFrameDisplay), cachedScopeObj(nullptr),
        hasInlineCaches(false), hasSuperReference(false), isDefaultConstructor(false), isActiveScript(false)
    {
        Assert(proxy->GetFunctionProxy() == proxy);
        Assert(proxy->EnsureDeferredPrototypeType() == deferredPrototypeType)
        DebugOnly(VerifyEntryPoint());

#if ENABLE_NATIVE_CODEGEN
#ifdef BGJIT_STATS
        if (!proxy->IsDeferred())
        {
            FunctionBody* body = proxy->GetFunctionBody();
            if(!body->GetNativeEntryPointUsed() &&
                body->GetDefaultFunctionEntryPointInfo()->IsCodeGenDone())
            {
                MemoryBarrier();

                type->GetScriptContext()->jitCodeUsed += body->GetByteCodeCount();
                type->GetScriptContext()->funcJitCodeUsed++;

                body->SetNativeEntryPointUsed(true);
            }
        }
#endif
#endif
    }

    ScriptFunction * ScriptFunction::OP_NewScFunc(FrameDisplay *environment, FunctionProxy** proxyRef)
    {
        AssertMsg(proxyRef!= nullptr, "BYTE-CODE VERIFY: Must specify a valid function to create");
        FunctionProxy* functionProxy = (*proxyRef);
        AssertMsg(functionProxy!= nullptr, "BYTE-CODE VERIFY: Must specify a valid function to create");

        ScriptContext* scriptContext = functionProxy->GetScriptContext();

        bool hasSuperReference = functionProxy->HasSuperReference();
        bool isDefaultConstructor = functionProxy->IsDefaultConstructor();

        if (functionProxy->IsFunctionBody() && functionProxy->GetFunctionBody()->GetInlineCachesOnFunctionObject())
        {
            Js::FunctionBody * functionBody = functionProxy->GetFunctionBody();
            ScriptFunctionWithInlineCache* pfuncScriptWithInlineCache = scriptContext->GetLibrary()->CreateScriptFunctionWithInlineCache(functionProxy);
            pfuncScriptWithInlineCache->SetEnvironment(environment);
            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_FUNCTION(pfuncScriptWithInlineCache, EtwTrace::GetFunctionId(functionProxy)));

            Assert(functionBody->GetInlineCacheCount() + functionBody->GetIsInstInlineCacheCount());

            if (functionBody->GetIsFirstFunctionObject())
            {
                // point the inline caches of the first function object to those on the function body.
                pfuncScriptWithInlineCache->SetInlineCachesFromFunctionBody();
                functionBody->SetIsNotFirstFunctionObject();
            }
            else
            {
                // allocate inline cache for this function object
                pfuncScriptWithInlineCache->CreateInlineCache();
            }

            pfuncScriptWithInlineCache->SetHasSuperReference(hasSuperReference);
            pfuncScriptWithInlineCache->SetIsDefaultConstructor(isDefaultConstructor);

            if (PHASE_TRACE1(Js::ScriptFunctionWithInlineCachePhase))
            {
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                Output::Print(L"Function object with inline cache: function number: (%s)\tfunction name: %s\n",
                    functionBody->GetDebugNumberSet(debugStringBuffer), functionBody->GetDisplayName());
                Output::Flush();
            }
            return pfuncScriptWithInlineCache;
        }
        else if(functionProxy->IsFunctionBody() && functionProxy->GetFunctionBody()->GetIsAsmJsFunction())
        {
            AsmJsScriptFunction* asmJsFunc = scriptContext->GetLibrary()->CreateAsmJsScriptFunction(functionProxy);
            asmJsFunc->SetEnvironment(environment);

            Assert(!hasSuperReference);
            asmJsFunc->SetHasSuperReference(hasSuperReference);
            asmJsFunc->SetIsDefaultConstructor(isDefaultConstructor);

            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_FUNCTION(asmJsFunc, EtwTrace::GetFunctionId(functionProxy)));

            return asmJsFunc;
        }
        else
        {
            ScriptFunction* pfuncScript = scriptContext->GetLibrary()->CreateScriptFunction(functionProxy);
            pfuncScript->SetEnvironment(environment);

            pfuncScript->SetHasSuperReference(hasSuperReference);
            pfuncScript->SetIsDefaultConstructor(isDefaultConstructor);

            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_FUNCTION(pfuncScript, EtwTrace::GetFunctionId(functionProxy)));

            return pfuncScript;
        }
    }

    void ScriptFunction::SetEnvironment(FrameDisplay * environment)
    {
        //Assert(ThreadContext::IsOnStack(this) || !ThreadContext::IsOnStack(environment));
        this->environment = environment;
    }

    void ScriptFunction::InvalidateCachedScopeChain()
    {
        // Note: Currently this helper assumes that we're in an eval-class case
        // where all the contents of the closure environment are dynamic objects.
        // Invalidating scopes that are raw slot arrays, etc., will have to be done
        // directly in byte code.

        // A function nested within this one has escaped.
        // Invalidate our own cached scope object, and walk the closure environment
        // doing this same.
        if (this->cachedScopeObj)
        {
            this->cachedScopeObj->InvalidateCachedScope();
        }
        FrameDisplay *pDisplay = this->environment;
        uint length = (uint)pDisplay->GetLength();
        for (uint i = 0; i < length; i++)
        {
            Var scope = pDisplay->GetItem(i);
            RecyclableObject *scopeObj = RecyclableObject::FromVar(scope);
            scopeObj->InvalidateCachedScope();
        }
    }

    bool ScriptFunction::Is(Var func)
    {
        return JavascriptFunction::Is(func) && JavascriptFunction::FromVar(func)->GetFunctionInfo()->HasBody();
    }

    ScriptFunction * ScriptFunction::FromVar(Var func)
    {
        Assert(ScriptFunction::Is(func));
        return reinterpret_cast<ScriptFunction *>(func);
    }

    ProxyEntryPointInfo * ScriptFunction::GetEntryPointInfo() const
    {
        return this->GetScriptFunctionType()->GetEntryPointInfo();
    }

    ScriptFunctionType * ScriptFunction::GetScriptFunctionType() const
    {
        return (ScriptFunctionType *)GetDynamicType();
    }

    ScriptFunctionType * ScriptFunction::DuplicateType()
    {
        ScriptFunctionType* type = RecyclerNew(this->GetScriptContext()->GetRecycler(),
            ScriptFunctionType, this->GetScriptFunctionType());

        this->GetFunctionProxy()->RegisterFunctionObjectType(type);

        return type;
    }

    uint32 ScriptFunction::GetFrameHeight(FunctionEntryPointInfo* entryPointInfo) const
    {
        Assert(this->GetFunctionBody() != nullptr);

        return this->GetFunctionBody()->GetFrameHeight(entryPointInfo);
    }

    bool ScriptFunction::HasFunctionBody()
    {
        // for asmjs we want to first check if the FunctionObject has a function body. Check that the function is not deferred
        return  !this->GetFunctionInfo()->IsDeferredParseFunction() && !this->GetFunctionInfo()->IsDeferredDeserializeFunction() && GetParseableFunctionInfo()->IsFunctionParsed();
    }

    void ScriptFunction::ChangeEntryPoint(ProxyEntryPointInfo* entryPointInfo, JavascriptMethod entryPoint)
    {
        Assert(entryPoint != nullptr);
        Assert(this->GetTypeId() == TypeIds_Function);
#if ENABLE_NATIVE_CODEGEN
        Assert(!IsCrossSiteObject() || entryPoint != (Js::JavascriptMethod)checkCodeGenThunk);
#else
        Assert(!IsCrossSiteObject());
#endif

        Assert((entryPointInfo != nullptr && this->GetFunctionProxy() != nullptr));
        if (this->GetEntryPoint() == entryPoint && this->GetScriptFunctionType()->GetEntryPointInfo() == entryPointInfo)
        {
            return;
        }

        bool isAsmJS = false;
        if (HasFunctionBody())
        {
            isAsmJS = this->GetFunctionBody()->GetIsAsmjsMode();
        }

        // ASMJS:- for asmjs we don't need to update the entry point here as it updates the types entry point
        if (!isAsmJS)
        {
            // We can't go from cross-site to non-cross-site. Update only in the non-cross site case
            if (!CrossSite::IsThunk(this->GetEntryPoint()))
            {
                this->SetEntryPoint(entryPoint);
            }
        }
        // instead update the address in the function entrypoint info
        else
        {
            entryPointInfo->address = entryPoint;
        }

        if (!isAsmJS)
        {
            ProxyEntryPointInfo* oldEntryPointInfo = this->GetScriptFunctionType()->GetEntryPointInfo();
            if (oldEntryPointInfo
                && oldEntryPointInfo != entryPointInfo
                && oldEntryPointInfo->SupportsExpiration())
            {
                // The old entry point could be executing so we need root it to make sure
                // it isn't prematurely collected. The rooting is done by queuing it up on the threadContext
                ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();

                threadContext->QueueFreeOldEntryPointInfoIfInScript((FunctionEntryPointInfo*)oldEntryPointInfo);
            }
        }

        this->GetScriptFunctionType()->SetEntryPointInfo(entryPointInfo);
    }

    FunctionProxy * ScriptFunction::GetFunctionProxy() const
    {
        Assert(this->functionInfo->HasBody());
        return reinterpret_cast<FunctionProxy *>(this->functionInfo);
    }
    JavascriptMethod ScriptFunction::UpdateUndeferredBody(FunctionBody* newFunctionInfo)
    {
        // Update deferred parsed/serialized function to the real function body
        Assert(this->functionInfo->HasBody());
        if (this->functionInfo != newFunctionInfo)
        {
            Assert(this->functionInfo->GetFunctionBody() == newFunctionInfo);
            Assert(!newFunctionInfo->IsDeferred());
            DynamicType * type = this->GetDynamicType();

            // If the type is shared, it must be the shared one in the old function proxy

            DebugOnly(FunctionProxy * oldProxy = this->GetFunctionProxy());
            this->functionInfo = newFunctionInfo;

            if (type->GetIsShared())
            {
                // if it is shared, it must still be the deferred prototype from the old proxy
                Assert(type == oldProxy->GetDeferredPrototypeType());

                // the type is still shared, we can't modify it, just migrate to the shared one in the function body
                this->ReplaceType(newFunctionInfo->EnsureDeferredPrototypeType());
            }
        }

        // The type has change from the default, it is not share, just use that one.
        JavascriptMethod directEntryPoint = newFunctionInfo->GetDirectEntryPoint(newFunctionInfo->GetDefaultEntryPointInfo());
        Assert(directEntryPoint != DefaultDeferredParsingThunk && directEntryPoint != ProfileDeferredParsingThunk);

        Js::FunctionEntryPointInfo* defaultEntryPointInfo = newFunctionInfo->GetDefaultFunctionEntryPointInfo();
        JavascriptMethod thunkEntryPoint = this->UpdateThunkEntryPoint(defaultEntryPointInfo,
                directEntryPoint);

        this->GetScriptFunctionType()->SetEntryPointInfo(defaultEntryPointInfo);

        return thunkEntryPoint;

    }

    JavascriptMethod ScriptFunction::UpdateThunkEntryPoint(FunctionEntryPointInfo* entryPointInfo, JavascriptMethod entryPoint)
    {
        this->ChangeEntryPoint(entryPointInfo, entryPoint);

        if (!CrossSite::IsThunk(this->GetEntryPoint()))
        {
            return entryPoint;
        }

        // We already pass through the cross site thunk, which would have called the profile thunk already if necessary
        // So just call the original entry point if our direct entry is the profile entry thunk
        // Otherwise, call the directEntryPoint which may have additional processing to do (e.g. ensure dynamic profile)
        Assert(this->IsCrossSiteObject());
        if (entryPoint != ProfileEntryThunk)
        {
            return entryPoint;
        }
        // Based on the comment below, this shouldn't be a a defer deserialization function as it would have a deferred thunk
        FunctionBody * functionBody = this->GetFunctionBody();
        // The original entry point should be an interpreter thunk or the native entry point;
        Assert(functionBody->IsInterpreterThunk() || functionBody->IsNativeOriginalEntryPoint());
        return functionBody->GetOriginalEntryPoint();
    }

    Var ScriptFunction::GetSourceString() const
    {
        return this->GetFunctionProxy()->EnsureDeserialized()->GetCachedSourceString();
    }

    Var ScriptFunction::FormatToString(JavascriptString* inputString)
    {
        FunctionProxy* proxy = this->GetFunctionProxy();
        ParseableFunctionInfo * pFuncBody = proxy->EnsureDeserialized();
        const wchar_t * inputStr = inputString->GetString();
        const wchar_t * paramStr = wcschr(inputStr, L'(');

        if (paramStr == nullptr || wcscmp(pFuncBody->GetDisplayName(), Js::Constants::EvalCode) == 0)
        {
            Assert(pFuncBody->IsEval());
            return inputString;
        }

        ScriptContext * scriptContext = this->GetScriptContext();
        JavascriptLibrary *javascriptLibrary = scriptContext->GetLibrary();
        bool isClassMethod = this->GetFunctionInfo()->IsClassMethod() || this->GetFunctionInfo()->IsClassConstructor();

        JavascriptString* prefixString = nullptr;
        uint prefixStringLength = 0;
        const wchar_t* name = L"";
        charcount_t nameLength = 0;
        Var returnStr = nullptr;

        if (!isClassMethod)
        {
            prefixString = javascriptLibrary->GetFunctionPrefixString();
            if (pFuncBody->IsGenerator())
            {
                prefixString = javascriptLibrary->GetGeneratorFunctionPrefixString();
            }
            prefixStringLength = prefixString->GetLength();

            if (pFuncBody->GetIsAccessor())
            {
                name = pFuncBody->GetShortDisplayName(&nameLength);

            }
            else if (pFuncBody->GetIsDeclaration() || pFuncBody->GetIsNamedFunctionExpression())
            {
                name = pFuncBody->GetDisplayName();
                nameLength = pFuncBody->GetDisplayNameLength();
                if (name == Js::Constants::FunctionCode)
                {
                    name = Js::Constants::Anonymous;
                    nameLength = Js::Constants::AnonymousLength;
                }

            }
        }
        else
        {

            if (this->GetFunctionInfo()->IsClassConstructor())
            {
                name = L"constructor";
                nameLength = _countof(L"constructor") -1; //subtract off \0
            }
            else
            {
                name = pFuncBody->GetShortDisplayName(&nameLength); //strip off prototype.
            }
        }

        ENTER_PINNED_SCOPE(JavascriptString, computedName);
        computedName = this->GetComputedName();
        if (computedName != nullptr)
        {
            prefixString = nullptr;
            prefixStringLength = 0;
            name = computedName->GetString();
            nameLength = computedName->GetLength();
        }

        uint functionBodyLength = inputString->GetLength() - ((uint)(paramStr - inputStr));
        size_t totalLength = prefixStringLength + functionBodyLength + nameLength;

        if (!IsValidCharCount(totalLength))
        {
            // We throw here because computed property names are evaluated at runtime and
            // thus are not a subset string of function body source (parameter inputString).
            // For all other cases totalLength <= inputString->GetLength().
            JavascriptExceptionOperators::ThrowOutOfMemory(this->GetScriptContext());
        }

        wchar_t * funcBodyStr = RecyclerNewArrayLeaf(this->GetScriptContext()->GetRecycler(), wchar_t, totalLength);
        wchar_t * funcBodyStrStart = funcBodyStr;
        if (prefixString != nullptr)
        {
            js_wmemcpy_s(funcBodyStr, prefixStringLength, prefixString->GetString(), prefixStringLength);
            funcBodyStrStart += prefixStringLength;
        }

        js_wmemcpy_s(funcBodyStrStart, nameLength, name, nameLength);
        funcBodyStrStart = funcBodyStrStart + nameLength;
        js_wmemcpy_s(funcBodyStrStart, functionBodyLength, paramStr, functionBodyLength);

        returnStr = LiteralString::NewCopyBuffer(funcBodyStr, (charcount_t)totalLength, scriptContext);

        LEAVE_PINNED_SCOPE();

        return returnStr;
    }

    Var ScriptFunction::EnsureSourceString()
    {
        // The function may be defer serialize, need to be deserialized
        FunctionProxy* proxy = this->GetFunctionProxy();
        ParseableFunctionInfo * pFuncBody = proxy->EnsureDeserialized();
        Var cachedSourceString = pFuncBody->GetCachedSourceString();
        if (cachedSourceString != nullptr)
        {
            return cachedSourceString;
        }

        ScriptContext * scriptContext = this->GetScriptContext();

        if (isDefaultConstructor)
        {
            PCWSTR fakeCode = hasSuperReference ? diagDefaultExtendsCtor : diagDefaultCtor;
            charcount_t fakeStrLen = hasSuperReference ? _countof(diagDefaultExtendsCtor) : _countof(diagDefaultCtor);
            Var fakeString = JavascriptString::NewCopyBuffer(fakeCode, fakeStrLen - 1, scriptContext);

            pFuncBody->SetCachedSourceString(fakeString);
            return fakeString;
        }

        //Library code should behave the same way as RuntimeFunctions
        Utf8SourceInfo* source = pFuncBody->GetUtf8SourceInfo();
        if (source != nullptr && source->GetIsLibraryCode())
        {
            //Don't display if it is anonymous function
            charcount_t displayNameLength = 0;
            PCWSTR displayName = pFuncBody->GetShortDisplayName(&displayNameLength);
            cachedSourceString = JavascriptFunction::GetLibraryCodeDisplayString(scriptContext, displayName);
        }
        else if (!pFuncBody->GetUtf8SourceInfo()->GetIsXDomain()
            // To avoid backward compat issue, we will not give out sourceString for function if it is called from
            // window.onerror trying to retrieve arguments.callee.caller.
            && !(pFuncBody->GetUtf8SourceInfo()->GetIsXDomainString() && scriptContext->GetThreadContext()->HasUnhandledException())
            )
        {
            // Decode UTF8 into Unicode
            // Consider: Should we have a JavascriptUtf8Substring class which defers decoding
            // until it's needed?

            BufferStringBuilder builder(pFuncBody->LengthInChars(), scriptContext);
            // TODO: What about surrogate pairs?
            utf8::DecodeOptions options = pFuncBody->GetUtf8SourceInfo()->IsCesu8() ? utf8::doAllowThreeByteSurrogates : utf8::doDefault;
            utf8::DecodeInto(builder.DangerousGetWritableBuffer(), pFuncBody->GetSource(L"ScriptFunction::EnsureSourceString"), pFuncBody->LengthInChars(), options);
            if (pFuncBody->IsLambda() || isActiveScript
#ifdef ENABLE_PROJECTION
                || scriptContext->GetConfig()->IsWinRTEnabled()
#endif
                )
            {
                cachedSourceString = builder.ToString();
            }
            else
            {
                cachedSourceString = FormatToString(builder.ToString());
            }
        }
        else
        {
            cachedSourceString = scriptContext->GetLibrary()->GetXDomainFunctionDisplayString();
        }
        Assert(cachedSourceString != nullptr);
        pFuncBody->SetCachedSourceString(cachedSourceString);
        return cachedSourceString;
    }

    BOOL ScriptFunction::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        if (!isDefaultConstructor)
        {
            return JavascriptFunction::GetDiagValueString(stringBuilder, requestContext);
        }

        if (hasSuperReference)
            stringBuilder->AppendCppLiteral(diagDefaultExtendsCtor);
        else
            stringBuilder->AppendCppLiteral(diagDefaultCtor);

        return TRUE;
    }

    AsmJsScriptFunction::AsmJsScriptFunction(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType) :
        ScriptFunction(proxy, deferredPrototypeType), m_moduleMemory(nullptr)
    {}

    AsmJsScriptFunction::AsmJsScriptFunction(DynamicType * type) :
        ScriptFunction(type), m_moduleMemory(nullptr)
    {}

    ScriptFunctionWithInlineCache::ScriptFunctionWithInlineCache(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType) :
        ScriptFunction(proxy, deferredPrototypeType), hasOwnInlineCaches(false)
    {}

    ScriptFunctionWithInlineCache::ScriptFunctionWithInlineCache(DynamicType * type) :
        ScriptFunction(type), hasOwnInlineCaches(false)
    {}

    bool ScriptFunctionWithInlineCache::Is(Var func)
    {
        return ScriptFunction::Is(func) && ScriptFunction::FromVar(func)->GetHasInlineCaches();
    }

    ScriptFunctionWithInlineCache* ScriptFunctionWithInlineCache::FromVar(Var func)
    {
        Assert(ScriptFunctionWithInlineCache::Is(func));
        return reinterpret_cast<ScriptFunctionWithInlineCache *>(func);
    }

    InlineCache * ScriptFunctionWithInlineCache::GetInlineCache(uint index)
    {
        Assert(this->m_inlineCaches != nullptr);
        Assert(index < this->GetInlineCacheCount());
#if DBG
        Assert(this->m_inlineCacheTypes[index] == InlineCacheTypeNone ||
            this->m_inlineCacheTypes[index] == InlineCacheTypeInlineCache);
        this->m_inlineCacheTypes[index] = InlineCacheTypeInlineCache;
#endif
        return reinterpret_cast<InlineCache *>(this->m_inlineCaches[index]);
    }

    void ScriptFunctionWithInlineCache::SetInlineCachesFromFunctionBody()
    {
        SetHasInlineCaches(true);
        Js::FunctionBody* functionBody = this->GetFunctionBody();
        this->m_inlineCaches = functionBody->GetInlineCaches();
#if DBG
        this->m_inlineCacheTypes = functionBody->GetInlineCacheTypes();
#endif
        this->rootObjectLoadInlineCacheStart = functionBody->GetRootObjectLoadInlineCacheStart();
        this->rootObjectLoadMethodInlineCacheStart = functionBody->GetRootObjectLoadMethodInlineCacheStart();
        this->rootObjectStoreInlineCacheStart = functionBody->GetRootObjectStoreInlineCacheStart();
        this->inlineCacheCount = functionBody->GetInlineCacheCount();
        this->isInstInlineCacheCount = functionBody->GetIsInstInlineCacheCount();
    }

    void ScriptFunctionWithInlineCache::CreateInlineCache()
    {
        Js::FunctionBody *functionBody = this->GetFunctionBody();
        this->rootObjectLoadInlineCacheStart = functionBody->GetRootObjectLoadInlineCacheStart();
        this->rootObjectStoreInlineCacheStart = functionBody->GetRootObjectStoreInlineCacheStart();
        this->inlineCacheCount = functionBody->GetInlineCacheCount();
        this->isInstInlineCacheCount = functionBody->GetIsInstInlineCacheCount();

        SetHasInlineCaches(true);
        AllocateInlineCache();
        hasOwnInlineCaches = true;
    }

    void ScriptFunctionWithInlineCache::Finalize(bool isShutdown)
    {
        if (isShutdown)
        {
            FreeOwnInlineCaches<true>();
        }
        else
        {
            FreeOwnInlineCaches<false>();
        }
    }
    template<bool isShutdown>
    void ScriptFunctionWithInlineCache::FreeOwnInlineCaches()
    {
        uint isInstInlineCacheStart = this->GetInlineCacheCount();
        uint totalCacheCount = isInstInlineCacheStart + isInstInlineCacheCount;
        if (this->GetHasInlineCaches() && this->m_inlineCaches && this->hasOwnInlineCaches)
        {
            Js::ScriptContext* scriptContext = this->GetFunctionBody()->GetScriptContext();
            uint i = 0;
            uint unregisteredInlineCacheCount = 0;
            uint plainInlineCacheEnd = rootObjectLoadInlineCacheStart;
            __analysis_assume(plainInlineCacheEnd < totalCacheCount);
            for (; i < plainInlineCacheEnd; i++)
            {
                if (this->m_inlineCaches[i])
                {
                    InlineCache* inlineCache = (InlineCache*)this->m_inlineCaches[i];
                    if (isShutdown)
                    {
                        memset(this->m_inlineCaches[i], 0, sizeof(InlineCache));
                    }
                    else if(!scriptContext->IsClosed())
                    {
                        if (inlineCache->RemoveFromInvalidationList())
                        {
                            unregisteredInlineCacheCount++;
                        }
                        AllocatorDelete(InlineCacheAllocator, scriptContext->GetInlineCacheAllocator(), inlineCache);
                    }
                    this->m_inlineCaches[i] = nullptr;
                }
            }

            i = isInstInlineCacheStart;
            for (; i < totalCacheCount; i++)
            {
                if (this->m_inlineCaches[i])
                {
                    if (isShutdown)
                    {
                        memset(this->m_inlineCaches[i], 0, sizeof(IsInstInlineCache));
                    }
                    else if (!scriptContext->IsClosed())
                    {
                        AllocatorDelete(IsInstInlineCacheAllocator, scriptContext->GetIsInstInlineCacheAllocator(), (IsInstInlineCache*)this->m_inlineCaches[i]);
                    }
                    this->m_inlineCaches[i] = nullptr;
                }
            }

            if (!isShutdown && unregisteredInlineCacheCount > 0 && !scriptContext->IsClosed())
            {
                scriptContext->GetThreadContext()->NotifyInlineCacheBatchUnregistered(unregisteredInlineCacheCount);
            }
        }
    }

    void ScriptFunctionWithInlineCache::AllocateInlineCache()
    {
        Assert(this->m_inlineCaches == nullptr);
        uint isInstInlineCacheStart = this->GetInlineCacheCount();
        uint totalCacheCount = isInstInlineCacheStart + isInstInlineCacheCount;
        Js::FunctionBody* functionBody = this->GetFunctionBody();

        if (totalCacheCount != 0)
        {
            // Root object inline cache are not leaf
            Js::ScriptContext* scriptContext = this->GetFunctionBody()->GetScriptContext();
            void ** inlineCaches = RecyclerNewArrayZ(scriptContext->GetRecycler() ,
                void*, totalCacheCount);
#if DBG
            this->m_inlineCacheTypes = RecyclerNewArrayLeafZ(scriptContext->GetRecycler(),
                byte, totalCacheCount);
#endif
            uint i = 0;
            uint plainInlineCacheEnd = rootObjectLoadInlineCacheStart;
            __analysis_assume(plainInlineCacheEnd <= totalCacheCount);
            for (; i < plainInlineCacheEnd; i++)
            {
                inlineCaches[i] = AllocatorNewZ(InlineCacheAllocator,
                    scriptContext->GetInlineCacheAllocator(), InlineCache);
            }
            Js::RootObjectBase * rootObject = functionBody->GetRootObject();
            ThreadContext * threadContext = scriptContext->GetThreadContext();
            uint rootObjectLoadInlineCacheEnd = rootObjectLoadMethodInlineCacheStart;
            __analysis_assume(rootObjectLoadInlineCacheEnd <= totalCacheCount);
            for (; i < rootObjectLoadInlineCacheEnd; i++)
            {
                inlineCaches[i] = rootObject->GetInlineCache(
                    threadContext->GetPropertyName(functionBody->GetPropertyIdFromCacheId(i)), false, false);
            }
            uint rootObjectLoadMethodInlineCacheEnd = rootObjectStoreInlineCacheStart;
            __analysis_assume(rootObjectLoadMethodInlineCacheEnd <= totalCacheCount);
            for (; i < rootObjectLoadMethodInlineCacheEnd; i++)
            {
                inlineCaches[i] = rootObject->GetInlineCache(
                    threadContext->GetPropertyName(functionBody->GetPropertyIdFromCacheId(i)), true, false);
            }
            uint rootObjectStoreInlineCacheEnd = isInstInlineCacheStart;
            __analysis_assume(rootObjectStoreInlineCacheEnd <= totalCacheCount);
            for (; i < rootObjectStoreInlineCacheEnd; i++)
            {
#pragma prefast(suppress:6386, "The analysis assume didn't help prefast figure out this is in range")
                inlineCaches[i] = rootObject->GetInlineCache(
                    threadContext->GetPropertyName(functionBody->GetPropertyIdFromCacheId(i)), false, true);
            }
            for (; i < totalCacheCount; i++)
            {
                inlineCaches[i] = AllocatorNewStructZ(IsInstInlineCacheAllocator,
                    functionBody->GetScriptContext()->GetIsInstInlineCacheAllocator(), IsInstInlineCache);
            }
#if DBG
            this->m_inlineCacheTypes = RecyclerNewArrayLeafZ(functionBody->GetScriptContext()->GetRecycler(),
                byte, totalCacheCount);
#endif
            this->m_inlineCaches = inlineCaches;
        }
    }

    bool ScriptFunction::GetSymbolName(const wchar_t** symbolName, charcount_t* length) const
    {
        if (nullptr != this->computedNameVar && JavascriptSymbol::Is(this->computedNameVar))
        {
            const PropertyRecord* symbolRecord = JavascriptSymbol::FromVar(this->computedNameVar)->GetValue();
            *symbolName = symbolRecord->GetBuffer();
            *length = symbolRecord->GetLength();
            return true;
        }
        *symbolName = nullptr;
        *length = 0;
        return false;
    }

    JavascriptString* ScriptFunction::GetDisplayNameImpl() const
    {
        Assert(this->GetFunctionProxy() != nullptr); // The caller should guarantee a proxy exists
        ParseableFunctionInfo * func = this->GetFunctionProxy()->EnsureDeserialized();
        const wchar_t* name = nullptr;
        charcount_t length = 0;
        JavascriptString* returnStr = nullptr;
        ENTER_PINNED_SCOPE(JavascriptString, computedName);

        if (computedNameVar != nullptr)
        {
            const wchar_t* symbolName = nullptr;
            charcount_t symbolNameLength = 0;
            if (this->GetSymbolName(&symbolName, &symbolNameLength))
            {
                if (symbolNameLength == 0)
                {
                    name = symbolName;
                }
                else
                {
                    name = FunctionProxy::WrapWithBrackets(symbolName, symbolNameLength, this->GetScriptContext());
                    length = symbolNameLength + 2; //adding 2 to length for  brackets
                }
            }
            else
            {
                computedName = this->GetComputedName();
                if (!func->GetIsAccessor())
                {
                    return computedName;
                }
                name = computedName->GetString();
                length = computedName->GetLength();
            }
        }
        else
        {
            name = Constants::Empty;
            if (func->GetIsNamedFunctionExpression()) // GetIsNamedFunctionExpression -> ex. var a = function foo() {} where name is foo
            {
                name = func->GetShortDisplayName(&length);
            }
            else if (func->GetIsNameIdentifierRef()) // GetIsNameIdentifierRef        -> confirms a name is not attached like o.x = function() {}
            {
                if (this->GetScriptContext()->GetConfig()->IsES6FunctionNameFullEnabled())
                {
                    name = func->GetShortDisplayName(&length);
                }
                else if (func->GetIsDeclaration() || // GetIsDeclaration -> ex. function foo () {}
                         func->GetIsAccessor()    || // GetIsAccessor    -> ex. var a = { get f() {}} new enough syntax that we do not have to disable by default
                         func->IsLambda()         || // IsLambda         -> ex. var y = { o : () => {}}
                         GetHomeObj())               // GetHomeObj       -> ex. var o = class {}, confirms this is a constructor or method on a class
                {
                    name = func->GetShortDisplayName(&length);
                }
            }
        }
        AssertMsg(IsValidCharCount(length), "JavascriptString can't be larger than charcount_t");
        returnStr = DisplayNameHelper(name, static_cast<charcount_t>(length));

        LEAVE_PINNED_SCOPE();

        return returnStr;
    }

    bool ScriptFunction::IsAnonymousFunction() const
    {
        return this->GetFunctionProxy()->GetIsAnonymousFunction();
    }

    JavascriptString* ScriptFunction::GetComputedName() const
    {
        JavascriptString* computedName = nullptr;
        ScriptContext* scriptContext = this->GetScriptContext();
        if (computedNameVar != nullptr)
        {
            if (TaggedInt::Is(computedNameVar))
            {
                computedName = TaggedInt::ToString(computedNameVar, scriptContext);
            }
            else
            {
                computedName = JavascriptConversion::ToString(computedNameVar, scriptContext);
            }
            return computedName;
        }
        return nullptr;
    }

    void ScriptFunctionWithInlineCache::ClearInlineCacheOnFunctionObject()
    {
        if (NULL != this->m_inlineCaches)
        {
            FreeOwnInlineCaches<false>();
            this->m_inlineCaches = NULL;
            this->inlineCacheCount = 0;
            this->rootObjectLoadInlineCacheStart = 0;
            this->rootObjectLoadMethodInlineCacheStart = 0;
            this->rootObjectStoreInlineCacheStart = 0;
            this->isInstInlineCacheCount = 0;
        }
        SetHasInlineCaches(false);
    }
}

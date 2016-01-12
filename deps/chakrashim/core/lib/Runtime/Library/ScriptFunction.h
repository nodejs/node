//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class ScriptFunctionBase : public JavascriptFunction
    {
    protected:
        ScriptFunctionBase(DynamicType * type);
        ScriptFunctionBase(DynamicType * type, FunctionInfo * functionInfo);

        DEFINE_VTABLE_CTOR(ScriptFunctionBase, JavascriptFunction);

    public:
        static bool Is(Var func);
        static ScriptFunctionBase * FromVar(Var func);

        virtual Var  GetHomeObj() const = 0;
        virtual void SetHomeObj(Var homeObj) = 0;
        virtual void SetComputedNameVar(Var computedNameVar) = 0;
        virtual Var GetComputedNameVar() const = 0;
        virtual bool IsAnonymousFunction() const = 0;
    };

    class ScriptFunction : public ScriptFunctionBase
    {
    private:
        FrameDisplay* environment;  // Optional environment, for closures
        ActivationObjectEx *cachedScopeObj;
        Var homeObj;
        Var computedNameVar;
        bool hasInlineCaches;
        bool hasSuperReference;
        bool isDefaultConstructor;
        bool isActiveScript;
        static const wchar_t diagDefaultCtor[];
        static const wchar_t diagDefaultExtendsCtor[];

        bool HasFunctionBody();
        Var FormatToString(JavascriptString* inputString);
    protected:
        ScriptFunction(DynamicType * type);

        DEFINE_VTABLE_CTOR(ScriptFunction, ScriptFunctionBase);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(ScriptFunction);
    public:
        ScriptFunction(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType);
        static bool Is(Var func);
        static ScriptFunction * FromVar(Var func);
        static ScriptFunction * OP_NewScFunc(FrameDisplay *environment, FunctionProxy** proxyRef);

        ProxyEntryPointInfo* GetEntryPointInfo() const;
        FunctionEntryPointInfo* GetFunctionEntryPointInfo() const
        {
            Assert(this->GetFunctionProxy()->IsDeferred() == FALSE);
            return (FunctionEntryPointInfo*) this->GetEntryPointInfo();
        }

        FunctionProxy * GetFunctionProxy() const;
        ScriptFunctionType * GetScriptFunctionType() const;

        uint32 GetFrameHeight(FunctionEntryPointInfo* entryPointInfo) const;
        FrameDisplay* GetEnvironment() const { return environment; }
        void SetEnvironment(FrameDisplay * environment);
        ActivationObjectEx *GetCachedScope() const { return cachedScopeObj; }
        void SetCachedScope(ActivationObjectEx *obj) { cachedScopeObj = obj; }
        void InvalidateCachedScopeChain();

        static uint32 GetOffsetOfEnvironment() { return offsetof(ScriptFunction, environment); }
        static uint32 GetOffsetOfCachedScopeObj() { return offsetof(ScriptFunction, cachedScopeObj); };
        static uint32 GetOffsetOfHasInlineCaches() { return offsetof(ScriptFunction, hasInlineCaches); };

        void ChangeEntryPoint(ProxyEntryPointInfo* entryPointInfo, JavascriptMethod entryPoint);
        JavascriptMethod UpdateThunkEntryPoint(FunctionEntryPointInfo* entryPointInfo, JavascriptMethod entryPoint);
        JavascriptMethod UpdateUndeferredBody(FunctionBody* newFunctionInfo);

        virtual ScriptFunctionType * DuplicateType() override;

        virtual Var GetSourceString() const;
        virtual Var EnsureSourceString();

        bool GetHasInlineCaches() { return hasInlineCaches; }
        void SetHasInlineCaches(bool has) { hasInlineCaches = has; }

        bool HasSuperReference() { return hasSuperReference; }
        void SetHasSuperReference(bool has) { hasSuperReference = has; }

        bool IsDefaultConstructor() { return isDefaultConstructor; }
        void SetIsDefaultConstructor(bool has) { isDefaultConstructor = has; }

        void SetIsActiveScript(bool is) { isActiveScript = is; }

        virtual Var GetHomeObj() const override { return homeObj; }
        virtual void SetHomeObj(Var homeObj) override { this->homeObj = homeObj; }
        virtual void SetComputedNameVar(Var computedNameVar) override { this->computedNameVar = computedNameVar; }
        bool GetSymbolName(const wchar_t** symbolName, charcount_t *length) const;
        virtual Var GetComputedNameVar() const override { return this->computedNameVar; }
        virtual JavascriptString* GetDisplayNameImpl() const;
        JavascriptString* GetComputedName() const;
        virtual bool IsAnonymousFunction() const override;
        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;

        virtual JavascriptFunction* GetRealFunctionObject() { return this; }
    };

    class AsmJsScriptFunction : public ScriptFunction
    {
    public:
        AsmJsScriptFunction(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType);

        void SetModuleMemory(Var* mem) { m_moduleMemory = mem; }
        Var * GetModuleMemory() const { return m_moduleMemory; }

        static uint32 GetOffsetOfModuleMemory() { return offsetof(AsmJsScriptFunction, m_moduleMemory); }
    protected:
        AsmJsScriptFunction(DynamicType * type);
        DEFINE_VTABLE_CTOR(AsmJsScriptFunction, ScriptFunction);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(AsmJsScriptFunction);

    private:
        Var * m_moduleMemory;
    };

    class ScriptFunctionWithInlineCache : public ScriptFunction
    {
    private:
        void** m_inlineCaches;
        bool hasOwnInlineCaches;

#if DBG
#define InlineCacheTypeNone         0x00
#define InlineCacheTypeInlineCache  0x01
#define InlineCacheTypeIsInst       0x02
        byte * m_inlineCacheTypes;
#endif
        uint inlineCacheCount;
        uint rootObjectLoadInlineCacheStart;
        uint rootObjectLoadMethodInlineCacheStart;
        uint rootObjectStoreInlineCacheStart;
        uint isInstInlineCacheCount;

    protected:
        ScriptFunctionWithInlineCache(DynamicType * type);

        DEFINE_VTABLE_CTOR(ScriptFunctionWithInlineCache, ScriptFunction);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(ScriptFunctionWithInlineCache);

    public:
        ScriptFunctionWithInlineCache(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType);
        static bool Is(Var func);
        static ScriptFunctionWithInlineCache * FromVar(Var func);
        void CreateInlineCache();
        void AllocateInlineCache();
        void ClearInlineCacheOnFunctionObject();
        InlineCache * GetInlineCache(uint index);
        uint GetInlineCacheCount() { return inlineCacheCount; }
        void** GetInlineCaches() { return m_inlineCaches; }
        void SetInlineCachesFromFunctionBody();
        static uint32 GetOffsetOfInlineCaches() { return offsetof(ScriptFunctionWithInlineCache, m_inlineCaches); };
        template<bool isShutdown>
        void FreeOwnInlineCaches();
        virtual void Finalize(bool isShutdown) override;
    };
} // namespace Js

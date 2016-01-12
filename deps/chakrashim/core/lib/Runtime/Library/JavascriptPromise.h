//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper
    {
        bool alreadyResolved;
    };

    class JavascriptPromiseResolveOrRejectFunction : public RuntimeFunction
    {
    protected:
        DEFINE_VTABLE_CTOR(JavascriptPromiseResolveOrRejectFunction, RuntimeFunction);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptPromiseResolveOrRejectFunction);

    public:
        JavascriptPromiseResolveOrRejectFunction(DynamicType* type);
        JavascriptPromiseResolveOrRejectFunction(DynamicType* type, FunctionInfo* functionInfo, JavascriptPromise* promise, bool isReject, JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper* alreadyResolvedRecord);

        inline static bool Is(Var var);
        inline static JavascriptPromiseResolveOrRejectFunction* FromVar(Var var);

        JavascriptPromise* GetPromise();
        bool IsRejectFunction();
        bool IsAlreadyResolved();
        void SetAlreadyResolved(bool is);

    private:
        JavascriptPromise* promise;
        bool isReject;
        JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper* alreadyResolvedWrapper;
    };

    class JavascriptPromiseAsyncSpawnExecutorFunction : public RuntimeFunction
    {
    protected:
        DEFINE_VTABLE_CTOR(JavascriptPromiseAsyncSpawnExecutorFunction, RuntimeFunction);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptPromiseAsyncSpawnExecutorFunction);

    public:
        JavascriptPromiseAsyncSpawnExecutorFunction(DynamicType* type, FunctionInfo* functionInfo, JavascriptGenerator* generatorFunction, Var target);

        inline static bool Is(Var var);
        inline static JavascriptPromiseAsyncSpawnExecutorFunction* FromVar(Var var);

        JavascriptGenerator* GetGeneratorFunction();
        Var GetTarget();

    private:
        JavascriptGenerator* generatorFunction;
        Var target; // this
    };

    class JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction : public RuntimeFunction
    {
    protected:
        DEFINE_VTABLE_CTOR(JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction, RuntimeFunction);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction);

    public:
        JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction(DynamicType* type, FunctionInfo* functionInfo, JavascriptGenerator* generator, Var argument, JavascriptFunction* resolve = NULL, JavascriptFunction* reject = NULL, bool isReject = false);

        inline static bool Is(Var var);
        inline static JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* FromVar(Var var);

        JavascriptGenerator* GetGenerator();
        JavascriptFunction* GetReject();
        JavascriptFunction* GetResolve();
        bool GetIsReject();
        Var GetArgument();

    private:
        JavascriptGenerator* generator;
        JavascriptFunction* reject;
        JavascriptFunction* resolve;
        bool isReject;
        Var argument;
    };

    class JavascriptPromiseCapabilitiesExecutorFunction : public RuntimeFunction
    {
    protected:
        DEFINE_VTABLE_CTOR(JavascriptPromiseCapabilitiesExecutorFunction, RuntimeFunction);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptPromiseCapabilitiesExecutorFunction);

    public:
        JavascriptPromiseCapabilitiesExecutorFunction(DynamicType* type, FunctionInfo* functionInfo, JavascriptPromiseCapability* capability);

        inline static bool Is(Var var);
        inline static JavascriptPromiseCapabilitiesExecutorFunction* FromVar(Var var);

        JavascriptPromiseCapability* GetCapability();

    private:
        JavascriptPromiseCapability* capability;
    };

    class JavascriptPromiseResolveThenableTaskFunction : public RuntimeFunction
    {
    protected:
        DEFINE_VTABLE_CTOR(JavascriptPromiseResolveThenableTaskFunction, RuntimeFunction);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptPromiseResolveThenableTaskFunction);

    public:
        JavascriptPromiseResolveThenableTaskFunction(DynamicType* type, FunctionInfo* functionInfo, JavascriptPromise* promise, RecyclableObject* thenable, RecyclableObject* thenFunction)
            : RuntimeFunction(type, functionInfo), promise(promise), thenable(thenable), thenFunction(thenFunction)
        { }

        inline static bool Is(Var var)
        {
            if (JavascriptFunction::Is(var))
            {
                JavascriptFunction* obj = JavascriptFunction::FromVar(var);

                return VirtualTableInfo<JavascriptPromiseResolveThenableTaskFunction>::HasVirtualTable(obj)
                    || VirtualTableInfo<CrossSiteObject<JavascriptPromiseResolveThenableTaskFunction>>::HasVirtualTable(obj);
            }

            return false;
        }

        inline static JavascriptPromiseResolveThenableTaskFunction* FromVar(Var var)
        {
            Assert(JavascriptPromiseResolveThenableTaskFunction::Is(var));

            return static_cast<JavascriptPromiseResolveThenableTaskFunction*>(var);
        }

        JavascriptPromise* GetPromise();
        RecyclableObject* GetThenable();
        RecyclableObject* GetThenFunction();


    private:
        JavascriptPromise* promise;
        RecyclableObject* thenable;
        RecyclableObject* thenFunction;
    };

    class JavascriptPromiseReactionTaskFunction : public RuntimeFunction
    {
    protected:
        DEFINE_VTABLE_CTOR(JavascriptPromiseReactionTaskFunction, RuntimeFunction);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptPromiseReactionTaskFunction);

    public:
        JavascriptPromiseReactionTaskFunction(DynamicType* type, FunctionInfo* functionInfo, JavascriptPromiseReaction* reaction, Var argument)
            : RuntimeFunction(type, functionInfo), reaction(reaction), argument(argument)
        { }

        inline static bool Is(Var var)
        {
            if (JavascriptFunction::Is(var))
            {
                JavascriptFunction* obj = JavascriptFunction::FromVar(var);

                return VirtualTableInfo<JavascriptPromiseReactionTaskFunction>::HasVirtualTable(obj)
                    || VirtualTableInfo<CrossSiteObject<JavascriptPromiseReactionTaskFunction>>::HasVirtualTable(obj);
            }

            return false;
        }

        inline static JavascriptPromiseReactionTaskFunction* FromVar(Var var)
        {
            Assert(JavascriptPromiseReactionTaskFunction::Is(var));

            return static_cast<JavascriptPromiseReactionTaskFunction*>(var);
        }

        JavascriptPromiseReaction* GetReaction();
        Var GetArgument();

    private:
        JavascriptPromiseReaction* reaction;
        Var argument;
    };

    struct JavascriptPromiseAllResolveElementFunctionRemainingElementsWrapper
    {
        uint32 remainingElements;
    };

    class JavascriptPromiseAllResolveElementFunction : public RuntimeFunction
    {
    protected:
        DEFINE_VTABLE_CTOR(JavascriptPromiseAllResolveElementFunction, RuntimeFunction);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptPromiseAllResolveElementFunction);

    public:
        JavascriptPromiseAllResolveElementFunction(DynamicType* type);
        JavascriptPromiseAllResolveElementFunction(DynamicType* type, FunctionInfo* functionInfo, uint32 index, JavascriptArray* values, JavascriptPromiseCapability* capabilities, JavascriptPromiseAllResolveElementFunctionRemainingElementsWrapper* remainingElementsWrapper);

        inline static bool Is(Var var);
        inline static JavascriptPromiseAllResolveElementFunction* FromVar(Var var);

        JavascriptPromiseCapability* GetCapabilities();
        uint32 GetIndex();
        uint32 GetRemainingElements();
        JavascriptArray* GetValues();
        bool IsAlreadyCalled() const;
        void SetAlreadyCalled(const bool is);

        uint32 DecrementRemainingElements();

    private:
        JavascriptPromiseCapability* capabilities;
        uint32 index;
        JavascriptPromiseAllResolveElementFunctionRemainingElementsWrapper* remainingElementsWrapper;
        JavascriptArray* values;
        bool alreadyCalled;
    };

    class JavascriptPromiseCapability : FinalizableObject
    {
    private:
        JavascriptPromiseCapability(Var promise, RecyclableObject* resolve, RecyclableObject* reject)
            : promise(promise), resolve(resolve), reject(reject)
        { }

    public:
        static JavascriptPromiseCapability* New(Var promise, RecyclableObject* resolve, RecyclableObject* reject, ScriptContext* scriptContext);

        RecyclableObject* GetResolve();
        RecyclableObject* GetReject();
        Var GetPromise();
        void SetPromise(Var);

        void SetResolve(RecyclableObject* resolve);
        void SetReject(RecyclableObject* reject);

    public:
        // Finalizable support
        virtual void Finalize(bool isShutdown)
        {
        }

        virtual void Dispose(bool isShutdown)
        {
        }

        virtual void Mark(Recycler *recycler) override
        {
            AssertMsg(false, "Mark called on object that isnt TrackableObject");
        }

    private:
        Var promise;
        RecyclableObject* resolve;
        RecyclableObject* reject;
    };

    typedef JsUtil::List<Js::JavascriptPromiseCapability*> JavascriptPromiseCapabilityList;

    class JavascriptPromiseReaction : FinalizableObject
    {
    private:
        JavascriptPromiseReaction(JavascriptPromiseCapability* capabilities, RecyclableObject* handler)
            : capabilities(capabilities), handler(handler)
        { }

    public:
        static JavascriptPromiseReaction* New(JavascriptPromiseCapability* capabilities, RecyclableObject* handler, ScriptContext* scriptContext);

        JavascriptPromiseCapability* GetCapabilities();
        RecyclableObject* GetHandler();

    public:
        // Finalizable support
        virtual void Finalize(bool isShutdown)
        {
        }

        virtual void Dispose(bool isShutdown)
        {
        }

        virtual void Mark(Recycler *recycler) override
        {
            AssertMsg(false, "Mark called on object that isnt TrackableObject");
        }

    private:
        JavascriptPromiseCapability* capabilities;
        RecyclableObject* handler;
    };

    typedef JsUtil::List<Js::JavascriptPromiseReaction*> JavascriptPromiseReactionList;

    class JavascriptPromise : public DynamicObject
    {
    private:
        DEFINE_VTABLE_CTOR(JavascriptPromise, DynamicObject);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptPromise);

    public:
        class EntryInfo
        {
        public:
            static FunctionInfo NewInstance;

            static FunctionInfo All;
            static FunctionInfo Catch;
            static FunctionInfo Race;
            static FunctionInfo Reject;
            static FunctionInfo Resolve;
            static FunctionInfo Then;

            static FunctionInfo Identity;
            static FunctionInfo Thrower;

            static FunctionInfo ResolveOrRejectFunction;
            static FunctionInfo AllResolveElementFunction;

            static FunctionInfo GetterSymbolSpecies;
        };

        JavascriptPromise(DynamicType * type);

        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryAll(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryCatch(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryRace(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryReject(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryResolve(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryThen(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryCapabilitiesExecutorFunction(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryResolveOrRejectFunction(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryReactionTaskFunction(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryResolveThenableTaskFunction(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIdentityFunction(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryThrowerFunction(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryAllResolveElementFunction(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryJavascriptPromiseAsyncSpawnExecutorFunction(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryJavascriptPromiseAsyncSpawnStepNextExecutorFunction(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryJavascriptPromiseAsyncSpawnStepThrowExecutorFunction(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryJavascriptPromiseAsyncSpawnCallStepExecutorFunction(RecyclableObject* function, CallInfo callInfo, ...);

        static bool Is(Var aValue);
        static JavascriptPromise* FromVar(Js::Var aValue);

        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;

        JavascriptPromiseReactionList* GetResolveReactions();
        JavascriptPromiseReactionList* GetRejectReactions();

        static JavascriptPromiseCapability* NewPromiseCapability(Var constructor, ScriptContext* scriptContext);
        static JavascriptPromiseCapability* CreatePromiseCapabilityRecord(Var promise, RecyclableObject* constructor, ScriptContext* scriptContext);
        static bool UpdatePromiseFromPotentialThenable(Var resolution, JavascriptPromiseCapability* promiseCapability, ScriptContext* scriptContext);
        static Var TriggerPromiseReactions(JavascriptPromiseReactionList* reactions, Var resolution, ScriptContext* scriptContext);
        static void EnqueuePromiseReactionTask(JavascriptPromiseReaction* reaction, Var resolution, ScriptContext* scriptContext);

        static void InitializePromise(JavascriptPromise* promise, JavascriptPromiseResolveOrRejectFunction** resolve, JavascriptPromiseResolveOrRejectFunction** reject, ScriptContext* scriptContext);

    protected:
        enum PromiseStatus
        {
            PromiseStatusCode_Undefined,
            PromiseStatusCode_Unresolved,
            PromiseStatusCode_HasResolution,
            PromiseStatusCode_HasRejection
        };

        PromiseStatus status;
        Var result;
        JavascriptPromiseReactionList* resolveReactions;
        JavascriptPromiseReactionList* rejectReactions;

    private :
        static void AsyncSpawnStep(JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* nextFunction, JavascriptGenerator* gen, JavascriptFunction* resolve, JavascriptFunction* reject);

    };
}

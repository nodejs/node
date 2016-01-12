//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Js
{
    class FunctionProxy;
    class FunctionBody;
    class ParseableFunctionInfo;
    class DeferDeserializeFunctionInfo;

    class FunctionInfo: public FinalizableObject
    {
        friend class RemoteFunctionBody;
    protected:
        DEFINE_VTABLE_CTOR_NOBASE(FunctionInfo);
    public:

        enum Attributes : uint32
        {
            None                           = 0x00000,
            ErrorOnNew                     = 0x00001,
            SkipDefaultNewObject           = 0x00002,
            DoNotProfile                   = 0x00004,
            HasNoSideEffect                = 0x00008, // calling function doesn’t cause an implicit flags to be set,
                                                      // the callee will detect and set implicit flags on its individual operations
            NeedCrossSiteSecurityCheck     = 0x00010,
            DeferredDeserialize            = 0x00020, // The function represents something that needs to be deserialized on use
            DeferredParse                  = 0x00040, // The function represents something that needs to be parsed on use
            CanBeHoisted                   = 0x00080, // The function return value won't be changed in a loop so the evaluation can be hoisted.
            SuperReference                 = 0x00100,
            ClassMethod                    = 0x00200, // The function is a class method
            ClassConstructor               = 0x00400, // The function is a class constructor
            DefaultConstructor             = 0x00800, // The function is a default class constructor
            Lambda                         = 0x01000,
            CapturesThis                   = 0x02000, // Only lambdas will set this; denotes whether the lambda referred to this, used by debugger
            Generator                      = 0x04000,
            BuiltInInlinableAsLdFldInlinee = 0x08000,
            Async                          = 0x10000
        };
        FunctionInfo(JavascriptMethod entryPoint, Attributes attributes = None, LocalFunctionId functionId = Js::Constants::NoFunctionId, FunctionBody* functionBodyImpl = NULL);

        static DWORD GetFunctionBodyImplOffset() { return offsetof(FunctionInfo, functionBodyImpl); }

        void VerifyOriginalEntryPoint() const;
        JavascriptMethod GetOriginalEntryPoint() const;
        JavascriptMethod GetOriginalEntryPoint_Unchecked() const;
        void SetOriginalEntryPoint(const JavascriptMethod originalEntryPoint);

        bool IsAsync() const { return ((this->attributes & Async) != 0); }
        bool IsDeferred() const { return ((this->attributes & (DeferredDeserialize | DeferredParse)) != 0); }
        bool IsLambda() const { return ((this->attributes & Lambda) != 0); }
        bool IsConstructor() const { return ((this->attributes & ErrorOnNew) == 0); }
        bool IsGenerator() const { return ((this->attributes & Generator) != 0); }
        bool IsDefaultConstructor() const { return ((this->attributes & DefaultConstructor) != 0); }
        bool IsClassConstructor() const { return ((this->attributes & ClassConstructor) != 0); }
        bool IsClassMethod() const { return ((this->attributes & ClassMethod) != 0); }
        bool HasSuperReference() const { return ((this->attributes & SuperReference) != 0); }

        BOOL HasBody() const { return functionBodyImpl != NULL; }
        BOOL HasParseableInfo() const { return this->HasBody() && !this->IsDeferredDeserializeFunction(); }

        FunctionProxy * GetFunctionProxy() const
        {
            return functionBodyImpl;
        }
        ParseableFunctionInfo* GetParseableFunctionInfo() const
        {
            Assert(functionBodyImpl == NULL || !IsDeferredDeserializeFunction());
            return (ParseableFunctionInfo*) functionBodyImpl;
        }
        ParseableFunctionInfo** GetParseableFunctionInfoRef() const
        {
            Assert(functionBodyImpl == NULL || !IsDeferredDeserializeFunction());
            return (ParseableFunctionInfo**)&functionBodyImpl;
        }
        DeferDeserializeFunctionInfo* GetDeferDeserializeFunctionInfo() const
        {
            Assert(functionBodyImpl == NULL || IsDeferredDeserializeFunction());
            return (DeferDeserializeFunctionInfo*)functionBodyImpl;
        }
        FunctionBody * GetFunctionBody() const;

        Attributes GetAttributes() const { return attributes; }
        static Attributes GetAttributes(Js::RecyclableObject * function);
        Js::LocalFunctionId GetLocalFunctionId() const { return functionId; }
        virtual void Finalize(bool isShutdown)
        {
        }

        virtual void Dispose(bool isShutdown)
        {
        }

        virtual void Mark(Recycler *recycler) override { AssertMsg(false, "Mark called on object that isn't TrackableObject"); }

        BOOL IsDeferredDeserializeFunction() const { return ((this->attributes & DeferredDeserialize) == DeferredDeserialize); }
        BOOL IsDeferredParseFunction() const { return ((this->attributes & DeferredParse) == DeferredParse); }

    protected:
        JavascriptMethod originalEntryPoint;
        // WriteBarrier-TODO: Fix this? This is used only by proxies to keep the deserialized version around
        // However, proxies are not allocated as write barrier memory currently so its fine to not set the write barrier for this field
        FunctionProxy * functionBodyImpl;     // Implementation of the function- null if the function doesn't have a body
        LocalFunctionId functionId;        // Per host source context (source file) function Id
        Attributes attributes;
    };

    // Helper FunctionInfo for builtins that we don't want to profile (script profiler).
    class NoProfileFunctionInfo : public FunctionInfo
    {
    public:
        NoProfileFunctionInfo(JavascriptMethod entryPoint)
            : FunctionInfo(entryPoint, Attributes::DoNotProfile)
        {}
    };
};

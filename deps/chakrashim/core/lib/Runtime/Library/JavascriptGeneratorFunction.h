//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class GeneratorVirtualScriptFunction;

    class JavascriptGeneratorFunction : public ScriptFunctionBase
    {
    private:
        static FunctionInfo functionInfo;
        GeneratorVirtualScriptFunction* scriptFunction;

        DEFINE_VTABLE_CTOR(JavascriptGeneratorFunction, ScriptFunctionBase);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptGeneratorFunction);

        bool GetPropertyBuiltIns(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext, BOOL* result);
        bool SetPropertyBuiltIns(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info, BOOL* result);

    protected:
        JavascriptGeneratorFunction(DynamicType* type);

    public:
        JavascriptGeneratorFunction(DynamicType* type, GeneratorVirtualScriptFunction* scriptFunction);

        virtual JavascriptString* GetDisplayNameImpl() const override;
        GeneratorVirtualScriptFunction* GetGeneratorVirtualScriptFunction() { return scriptFunction; }

        static JavascriptGeneratorFunction* FromVar(Var var);
        static bool Is(Var var);

        static JavascriptGeneratorFunction* OP_NewScGenFunc(FrameDisplay* environment, FunctionProxy** proxyRef);
        static Var EntryGeneratorFunctionImplementation(RecyclableObject* function, CallInfo callInfo, ...);

        virtual Var GetHomeObj() const override;
        virtual void SetHomeObj(Var homeObj) override;
        virtual void SetComputedNameVar(Var computedNameVar) override;
        virtual Var GetComputedNameVar() const override;
        virtual bool IsAnonymousFunction() const override;

        virtual Var GetSourceString() const;
        virtual Var EnsureSourceString();

        virtual BOOL HasProperty(PropertyId propertyId) override;
        virtual BOOL GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;

        virtual BOOL GetAccessors(PropertyId propertyId, Var *getter, Var *setter, ScriptContext * requestContext) override;
        virtual DescriptorFlags GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual DescriptorFlags GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;

        virtual BOOL InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags = PropertyOperation_None, PropertyValueInfo* info = NULL) override;
        virtual BOOL DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags) override;

        virtual BOOL IsWritable(PropertyId propertyId) override;
        virtual BOOL IsEnumerable(PropertyId propertyId) override;
        virtual bool IsGeneratorFunction() const { return true; };

        class EntryInfo
        {
        public:
            static FunctionInfo NewInstance;
        };

        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
    };

    class GeneratorVirtualScriptFunction : public ScriptFunction
    {
    private:
        friend class JavascriptGeneratorFunction;
        friend Var Js::JavascriptFunction::NewInstanceHelper(ScriptContext*, RecyclableObject*, CallInfo, ArgumentReader&, bool);

        JavascriptGeneratorFunction* realFunction;

        void SetRealGeneratorFunction(JavascriptGeneratorFunction* realFunction) { this->realFunction = realFunction; }

    public:
        GeneratorVirtualScriptFunction(FunctionProxy* proxy, ScriptFunctionType* deferredPrototypeType) : ScriptFunction(proxy, deferredPrototypeType) { }

        static uint32 GetRealFunctionOffset() { return offsetof(GeneratorVirtualScriptFunction, realFunction); }

        virtual JavascriptFunction* GetRealFunctionObject() override { return realFunction; }
    };
}

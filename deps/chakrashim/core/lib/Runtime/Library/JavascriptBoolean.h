//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class JavascriptBoolean sealed : public RecyclableObject
    {
    private:
        BOOL value;

        DEFINE_VTABLE_CTOR(JavascriptBoolean, RecyclableObject);
    public:
        JavascriptBoolean(BOOL val, StaticType * type) : RecyclableObject(type), value(val)
        {
            Assert(type->GetTypeId() == TypeIds_Boolean);
        }

        inline BOOL GetValue() { return value; }

        static inline bool Is(Var aValue);
        static inline JavascriptBoolean* FromVar(Var aValue);
        static Var ToVar(BOOL fValue,ScriptContext* scriptContext);

        class EntryInfo
        {
        public:
            static FunctionInfo NewInstance;
            static FunctionInfo ValueOf;
            static FunctionInfo ToString;
        };

        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryValueOf(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToString(RecyclableObject* function, CallInfo callInfo, ...);

        static Var OP_LdTrue(ScriptContext* scriptContext);
        static Var OP_LdFalse(ScriptContext* scriptContext);

        virtual BOOL Equals(Var other, BOOL* value, ScriptContext * requestContext) override;
        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual RecyclableObject* ToObject(ScriptContext * requestContext) override;
        virtual Var GetTypeOfString(ScriptContext * requestContext) override;
        // should never be called, JavascriptConversion::ToPrimitive() short-circuits and returns input value
        virtual BOOL ToPrimitive(JavascriptHint hint, Var* value, ScriptContext* requestContext) override {AssertMsg(false, "Boolean ToPrimitive should not be called"); *value = this; return true;}
        virtual RecyclableObject * CloneToScriptContext(ScriptContext* requestContext) override;

    private:
        static BOOL Equals(JavascriptBoolean* left, Var right, BOOL* value, ScriptContext * requestContext);
        static Var TryInvokeRemotelyOrThrow(JavascriptMethod entryPoint, ScriptContext * scriptContext, Arguments & args, long errorCode, PCWSTR varName);
    };
}

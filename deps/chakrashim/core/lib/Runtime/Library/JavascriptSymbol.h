//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class JavascriptSymbol sealed : public RecyclableObject
    {
    private:
        const PropertyRecord* value;

        DEFINE_VTABLE_CTOR(JavascriptSymbol, RecyclableObject);
    public:
        JavascriptSymbol(const PropertyRecord* val, StaticType* type) : RecyclableObject(type), value(val)
        {
            Assert(type->GetTypeId() == TypeIds_Symbol);
        }

        const PropertyRecord* GetValue() { return value; }

        static bool Is(Var aValue);
        static JavascriptSymbol* FromVar(Var aValue);

        class EntryInfo
        {
        public:
            static FunctionInfo NewInstance;
            static FunctionInfo ValueOf;
            static FunctionInfo ToString;
            static FunctionInfo For;
            static FunctionInfo KeyFor;

            static FunctionInfo SymbolToPrimitive;
        };

        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryValueOf(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFor(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryKeyFor(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySymbolToPrimitive(RecyclableObject* function, CallInfo callInfo, ...);

        virtual BOOL Equals(Var other, BOOL* value, ScriptContext * requestContext) override;
        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual RecyclableObject* ToObject(ScriptContext * requestContext) override;
        virtual Var GetTypeOfString(ScriptContext * requestContext) override;
        virtual BOOL ToPrimitive(JavascriptHint hint, Var* value, ScriptContext* requestContext) override { AssertMsg(false, "Symbol ToPrimitive should never be called, JavascriptConversion::ToPrimitive() short-circuits and returns input value"); *value = this; return true; }
        virtual RecyclableObject * CloneToScriptContext(ScriptContext* requestContext) override;

        JavascriptString* ToString(ScriptContext * requestContext);
        static JavascriptString* ToString(const PropertyRecord* propertyRecord, ScriptContext * requestContext);

    private:
        static BOOL Equals(JavascriptSymbol* left, Var right, BOOL* value, ScriptContext * requestContext);
        static Var TryInvokeRemotelyOrThrow(JavascriptMethod entryPoint, ScriptContext * scriptContext, Arguments & args, long errorCode, PCWSTR varName);
    };
}

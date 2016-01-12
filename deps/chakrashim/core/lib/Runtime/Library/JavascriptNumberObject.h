//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class JavascriptNumberObject : public DynamicObject
    {
    private:
        Var value;

        DEFINE_VTABLE_CTOR(JavascriptNumberObject, DynamicObject);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptNumberObject);
    protected:
        JavascriptNumberObject(DynamicType * type);

    public:
        JavascriptNumberObject(Var value, DynamicType * type);
        static bool Is(Var aValue);
        static JavascriptNumberObject* FromVar(Var aValue);
        double GetValue() const;
        void SetValue(Var value);
        Var Unwrap() const;

        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
    };
}

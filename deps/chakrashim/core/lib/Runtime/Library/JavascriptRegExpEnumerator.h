//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class JavascriptRegExpEnumerator : public JavascriptEnumerator
    {
    private:
        JavascriptRegExpConstructor* regExpObject;
        uint index;
        BOOL enumNonEnumerable;
    protected:
        DEFINE_VTABLE_CTOR(JavascriptRegExpEnumerator, JavascriptEnumerator);
        DEFINE_MARSHAL_ENUMERATOR_TO_SCRIPT_CONTEXT(JavascriptRegExpEnumerator);

    public:
        JavascriptRegExpEnumerator(JavascriptRegExpConstructor* regExpObject, ScriptContext * requestContext, BOOL enumNonEnumerable);
        virtual Var GetCurrentIndex() override;
        virtual Var GetCurrentValue() override;
        virtual BOOL MoveNext(PropertyAttributes* attributes = nullptr) override;
        virtual void Reset() override;
        virtual Var GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes = nullptr) override;
    };

    class JavascriptRegExpObjectEnumerator : public JavascriptEnumerator
    {
    private:
        JavascriptRegExpEnumerator* regExpEnumerator;
        JavascriptRegExpConstructor* regExpObject;
        JavascriptEnumerator* objectEnumerator;
        BOOL enumNonEnumerable;
        bool enumSymbols;

    protected:
        DEFINE_VTABLE_CTOR(JavascriptRegExpObjectEnumerator, JavascriptEnumerator);
        DEFINE_MARSHAL_ENUMERATOR_TO_SCRIPT_CONTEXT(JavascriptRegExpObjectEnumerator);

    public:
        JavascriptRegExpObjectEnumerator(JavascriptRegExpConstructor* regExpObject, ScriptContext * requestContext, BOOL enumNonEnumerable, bool enumSymbols);
        virtual Var GetCurrentIndex() override;
        virtual Var GetCurrentValue() override;
        virtual BOOL MoveNext(PropertyAttributes* attributes = nullptr) override;
        virtual void Reset() override;
        virtual bool GetCurrentPropertyId(PropertyId *propertyId) override;
        virtual Var GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes = nullptr) override;
    };
}

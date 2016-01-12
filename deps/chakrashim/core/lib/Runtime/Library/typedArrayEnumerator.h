//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class TypedArrayEnumerator : public JavascriptEnumerator
    {
    private:
        TypedArrayBase* typedArrayObject;
        uint32 index;
        bool doneArray;
        bool doneObject;
        BOOL enumNonEnumerable;
        bool enumSymbols;
        JavascriptEnumerator* objectEnumerator;

    protected:
        DEFINE_VTABLE_CTOR(TypedArrayEnumerator, JavascriptEnumerator);
        DEFINE_MARSHAL_ENUMERATOR_TO_SCRIPT_CONTEXT(TypedArrayEnumerator);

    public:
        TypedArrayEnumerator(BOOL enumNonEnumerable, TypedArrayBase* typeArrayBase, ScriptContext* scriptContext, bool enumSymbols = false);
        virtual Var GetCurrentIndex() override;
        virtual Var GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes = nullptr) override;
        virtual Var GetCurrentValue() override;
        virtual BOOL MoveNext(PropertyAttributes* attributes = nullptr) override;
        virtual void Reset() override;
        virtual bool GetCurrentPropertyId(PropertyId *propertyId) override;
    };
}

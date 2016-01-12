//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class ES5ArrayEnumerator : public JavascriptArrayEnumeratorBase
    {
    private:
        Var originalInstance;                   // The containing object or the standalone array itself
        uint32 initialLength;                   // The initial array length when this enumerator is created
        uint32 dataIndex;                       // Current data index
        uint32 descriptorIndex;                 // Current descriptor index
        IndexPropertyDescriptor* descriptor;    // Current descriptor
        void * descriptorValidationToken;
    protected:
        DEFINE_VTABLE_CTOR(ES5ArrayEnumerator, JavascriptArrayEnumeratorBase);
        DEFINE_MARSHAL_ENUMERATOR_TO_SCRIPT_CONTEXT(ES5ArrayEnumerator);

    private:
        ES5Array* GetArray() const { return ES5Array::FromVar(arrayObject); }

    public:
        ES5ArrayEnumerator(Var originalInstance, ES5Array* arrayObject, ScriptContext* scriptContext, BOOL enumNonEnumerable, bool enumSymbols = false);
        virtual Var GetCurrentIndex() override;
        virtual Var GetCurrentValue() override;
        virtual void Reset() override;
        virtual Var GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes = nullptr) override;
    };
}

//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

namespace Js
{
    // Enumerator for undefined/number/double values.
    class NullEnumerator : public JavascriptEnumerator
    {
    private:
        virtual Var GetCurrentIndex() override;
        virtual Var GetCurrentValue() override;
        virtual BOOL MoveNext(PropertyAttributes* attributes = nullptr) override;
        virtual void Reset() override;
        virtual Var GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes = nullptr) override;
        virtual bool GetCurrentPropertyId(PropertyId *propertyId) override;

    protected:
        DEFINE_VTABLE_CTOR(NullEnumerator, JavascriptEnumerator);

        // nothing to marshal
        virtual void MarshalToScriptContext(Js::ScriptContext * scriptContext) override {}
    public:
        NullEnumerator(ScriptContext* scriptContext) : JavascriptEnumerator(scriptContext) {}
    };
};

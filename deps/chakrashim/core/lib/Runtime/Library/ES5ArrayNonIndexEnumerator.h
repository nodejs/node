//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    //
    // Enumerates only non-index named properties.
    //
    class ES5ArrayNonIndexEnumerator : public ES5ArrayEnumerator
    {
    protected:
        DEFINE_VTABLE_CTOR(ES5ArrayNonIndexEnumerator, ES5ArrayEnumerator);
        DEFINE_MARSHAL_ENUMERATOR_TO_SCRIPT_CONTEXT(ES5ArrayNonIndexEnumerator);

    public:
        ES5ArrayNonIndexEnumerator(Var originalInstance, ES5Array* arrayObject, ScriptContext* scriptContext, BOOL enumNonEnumerable, bool enumSymbols = false)
            : ES5ArrayEnumerator(originalInstance, arrayObject, scriptContext, enumNonEnumerable, enumSymbols)
        {
            doneArray = true;
        }

        virtual void Reset() override
        {
            __super::Reset();
            doneArray = true;
        }
    };
}

//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    //
    // This enumerator only enumerates non-array-index named properties.
    //
    class JavascriptArrayNonIndexSnapshotEnumerator: public JavascriptArraySnapshotEnumerator
    {
    private:
        uint32 initialLength;

    protected:
        DEFINE_VTABLE_CTOR(JavascriptArrayNonIndexSnapshotEnumerator, JavascriptArraySnapshotEnumerator);
        DEFINE_MARSHAL_ENUMERATOR_TO_SCRIPT_CONTEXT(JavascriptArrayNonIndexSnapshotEnumerator);

    public:
        JavascriptArrayNonIndexSnapshotEnumerator(JavascriptArray* arrayObject, ScriptContext* scriptContext, BOOL enumNonEnumerable, bool enumSymbols = false)
            : JavascriptArraySnapshotEnumerator(arrayObject, scriptContext, enumNonEnumerable, enumSymbols)
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

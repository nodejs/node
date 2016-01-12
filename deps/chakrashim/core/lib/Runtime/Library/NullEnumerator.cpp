//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#include "Library\NullEnumerator.h"

namespace Js
{
    Var NullEnumerator::GetCurrentIndex()
    {
        // This function may be called without calling MoveNext to verify element availbility
        // by JavascriptDispatch::GetNextDispIDWithScriptEnter which call
        // GetEnumeratorCurrentPropertyId to resume an enumeration
        return this->GetLibrary()->GetUndefined();
    }

    Var NullEnumerator::GetCurrentValue()
    {
        Assert(false);
        return this->GetLibrary()->GetUndefined();
    }

    BOOL NullEnumerator::MoveNext(PropertyAttributes* attributes)
    {
        return FALSE;
    }

    void NullEnumerator::Reset()
    {
    }

    Var NullEnumerator::GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {
        return NULL;
    }

    bool NullEnumerator::GetCurrentPropertyId(PropertyId *propertyId)
    {
        return false;
    }
};

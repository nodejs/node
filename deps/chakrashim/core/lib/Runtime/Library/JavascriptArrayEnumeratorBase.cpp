//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptArrayEnumeratorBase::JavascriptArrayEnumeratorBase(
        JavascriptArray* arrayObject, ScriptContext* scriptContext, BOOL enumNonEnumerable, bool enumSymbols) :
        JavascriptEnumerator(scriptContext),
        arrayObject(arrayObject),
        enumNonEnumerable(enumNonEnumerable),
        enumSymbols(enumSymbols)
    {
    }

    Var JavascriptArrayEnumeratorBase::GetCurrentIndex()
    {
        if (index != JavascriptArray::InvalidIndex && !doneArray)
        {
            return arrayObject->GetScriptContext()->GetIntegerString(index);
        }
        else if (!doneObject)
        {
            return objectEnumerator->GetCurrentIndex();
        }
        else
        {
            return GetLibrary()->GetUndefined();
        }
    }

    Var JavascriptArrayEnumeratorBase::GetCurrentValue()
    {
        if (index != JavascriptArray::InvalidIndex && !doneArray)
        {
            Var element;
            if (arrayObject->DirectGetItemAtFull(index, &element))
            {
                return element;
            }
            else
            {
                return arrayObject->GetLibrary()->GetUndefined();
            }
        }
        else if (!doneObject)
        {
            return objectEnumerator->GetCurrentValue();
        }
        else
        {
            return GetLibrary()->GetUndefined();
        }
    }

    BOOL JavascriptArrayEnumeratorBase::MoveNext(PropertyAttributes* attributes)
    {
        PropertyId propId;
        return GetCurrentAndMoveNext(propId, attributes) != nullptr;
    }

    bool JavascriptArrayEnumeratorBase::GetCurrentPropertyId(PropertyId *pPropertyId)
    {
        if (index != JavascriptArray::InvalidIndex && !doneArray)
        {
            *pPropertyId = Constants::NoProperty;
            return false;
        }
        else if (!doneObject)
        {
            return objectEnumerator->GetCurrentPropertyId(pPropertyId);
        }
        else
        {
            *pPropertyId = Constants::NoProperty;
            return false;
        }
    }

}

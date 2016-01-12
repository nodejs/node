//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptArraySnapshotEnumerator::JavascriptArraySnapshotEnumerator(
        JavascriptArray* arrayObject, ScriptContext* scriptContext, BOOL enumNonEnumerable, bool enumSymbols) :
        JavascriptArrayEnumeratorBase(arrayObject, scriptContext, enumNonEnumerable, enumSymbols),
        initialLength(arrayObject->GetLength())
    {
        Reset();
    }

    Var JavascriptArraySnapshotEnumerator::GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {
        propertyId = Constants::NoProperty;

        if (!doneArray)
        {
            uint32 lastIndex = index;
            index = arrayObject->GetNextIndex(index);
            if (index >= initialLength) // End of array
            {
                index = lastIndex;
                doneArray = true;
            }
            else
            {
                if (attributes != nullptr)
                {
                    *attributes = PropertyEnumerable;
                }

                return arrayObject->GetScriptContext()->GetIntegerString(index);
            }
        }
        if (!doneObject)
        {
            Var currentIndex = objectEnumerator->GetCurrentAndMoveNext(propertyId, attributes);
            if (!currentIndex)
            {
                doneObject = true;
            }
            return currentIndex;
        }
        return nullptr;
    }

    void JavascriptArraySnapshotEnumerator::Reset()
    {
        index = JavascriptArray::InvalidIndex;
        doneArray = false;
        doneObject = false;

        Var enumerator;
        arrayObject->DynamicObject::GetEnumerator(enumNonEnumerable, &enumerator, GetScriptContext(), true, enumSymbols);
        objectEnumerator = (JavascriptEnumerator*)enumerator;
        initialLength = arrayObject->GetLength();
    }
}

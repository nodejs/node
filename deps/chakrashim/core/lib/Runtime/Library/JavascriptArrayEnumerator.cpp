//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptArrayEnumerator::JavascriptArrayEnumerator(
        JavascriptArray* arrayObject, ScriptContext* scriptContext, BOOL enumNonEnumerable, bool enumSymbols) :
        JavascriptArrayEnumeratorBase(arrayObject, scriptContext, enumNonEnumerable, enumSymbols)
    {
        Reset();
    }

    Var JavascriptArrayEnumerator::GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {
        // TypedArrayEnumerator follow the same logic but implementation is slightly
        // different as we don't have sparse array in typed array, and typed array
        // is DynamicObject instead of JavascriptArray.
        propertyId = Constants::NoProperty;

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arrayObject);
#endif
        if (!doneArray)
        {
            while (true)
            {
                uint32 lastIndex = index;
                index = arrayObject->GetNextIndex(index);
                if (index == JavascriptArray::InvalidIndex) // End of array
                {
                    index = lastIndex;
                    doneArray = true;
                    break;
                }

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
            if (currentIndex)
            {
                return currentIndex;
            }
            doneObject = true;
        }
        return nullptr;
    }

    void JavascriptArrayEnumerator::Reset()
    {
        index = JavascriptArray::InvalidIndex;
        doneArray = false;
        doneObject = false;
        Var enumerator;
        arrayObject->DynamicObject::GetEnumerator(enumNonEnumerable, &enumerator, GetScriptContext(), false, enumSymbols);
        objectEnumerator = (JavascriptEnumerator*)enumerator;
    }
}

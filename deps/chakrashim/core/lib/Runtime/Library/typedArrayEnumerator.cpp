//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    TypedArrayEnumerator::TypedArrayEnumerator(BOOL enumNonEnumerable, TypedArrayBase* typedArrayBase, ScriptContext* scriptContext, bool enumSymbols) :
        JavascriptEnumerator(scriptContext),
        typedArrayObject(typedArrayBase),
        enumNonEnumerable(enumNonEnumerable),
        enumSymbols(enumSymbols)
        {
            Reset();
        }

    Var TypedArrayEnumerator::GetCurrentIndex()
    {
        if (index != JavascriptArray::InvalidIndex && !doneArray)
        {
            return typedArrayObject->GetScriptContext()->GetIntegerString(index);
        }
        else if (!doneObject)
        {
            return objectEnumerator->GetCurrentIndex();
        }
        else
        {
            return typedArrayObject->GetType()->GetLibrary()->GetUndefined();
        }
    }

    BOOL TypedArrayEnumerator::MoveNext(PropertyAttributes* attributes)
    {
        PropertyId propId;
        return GetCurrentAndMoveNext(propId, attributes) != nullptr;
    }

    Var TypedArrayEnumerator::GetCurrentValue()
    {
        return GetLibrary()->GetUndefined();
    }

    Var TypedArrayEnumerator::GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {
        // TypedArrayEnumerator follows the same logic in JavascriptArrayEnumerator,
        // but the implementation is slightly different as we don't have sparse array
        // in typed array, and typed array is a DynamicObject instead of JavascriptArray.
        propertyId = Constants::NoProperty;
        ScriptContext *scriptContext = this->GetScriptContext();

        if (!doneArray)
        {
            while (true)
            {
                uint32 lastIndex = index;
                index++;
                if ((uint32)index >= typedArrayObject->GetLength()) // End of array
                {
                    index = lastIndex;
                    doneArray = true;
                    break;
                }

                if (attributes != nullptr)
                {
                    *attributes = PropertyEnumerable;
                }

                return scriptContext->GetIntegerString(index);
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

    void TypedArrayEnumerator::Reset()
    {
        index = JavascriptArray::InvalidIndex;
        doneArray = false;
        doneObject = false;
        Var enumerator;
        typedArrayObject->DynamicObject::GetEnumerator(enumNonEnumerable, &enumerator, GetScriptContext(), true, enumSymbols);
        objectEnumerator = (JavascriptEnumerator*)enumerator;
    }

    bool TypedArrayEnumerator::GetCurrentPropertyId(PropertyId* propertyId)
    {
        if (!doneArray)
        {
            *propertyId = Constants::NoProperty;
            return false;
        }
        if (!doneObject)
        {
            return objectEnumerator->GetCurrentPropertyId(propertyId);
        }
        *propertyId = Constants::NoProperty;
        return false;
    }
}

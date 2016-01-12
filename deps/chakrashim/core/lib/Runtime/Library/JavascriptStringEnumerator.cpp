//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptStringEnumerator::JavascriptStringEnumerator(JavascriptString* stringObject, ScriptContext * requestContext) :
        JavascriptEnumerator(requestContext),
        stringObject(stringObject),
        index(-1)
    {
    }

    Var JavascriptStringEnumerator::GetCurrentIndex()
    {
        ScriptContext *scriptContext = stringObject->GetScriptContext();

        if (index >= 0 && index < stringObject->GetLengthAsSignedInt())
        {
            return scriptContext->GetIntegerString(index);
        }
        else
        {
            return scriptContext->GetLibrary()->GetUndefined();
        }
    }

    Var JavascriptStringEnumerator::GetCurrentValue()
    {
        ScriptContext* scriptContext = stringObject->GetScriptContext();

        if (index >= 0 && index < stringObject->GetLengthAsSignedInt())
        {
            Var item;
            if (stringObject->GetItemAt(index, &item))
            {
                return item;
            }
        }

        return scriptContext->GetLibrary()->GetUndefined();
    }

    BOOL JavascriptStringEnumerator::MoveNext(PropertyAttributes* attributes)
    {
        if (++index < stringObject->GetLengthAsSignedInt())
        {
            if (attributes != nullptr)
            {
                *attributes = PropertyEnumerable;
            }

            return true;
        }
        else
        {
            index = stringObject->GetLength();
            return false;
        }
    }

    void JavascriptStringEnumerator::Reset()
    {
        index = -1;
    }


    Var JavascriptStringEnumerator::GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {
        propertyId = Constants::NoProperty;
        if (++index < stringObject->GetLengthAsSignedInt())
        {
            if (attributes != nullptr)
            {
                *attributes = PropertyEnumerable;
            }

            return stringObject->GetScriptContext()->GetIntegerString(index);
        }
        else
        {
            index = stringObject->GetLength();
            return nullptr;
        }
    }

    JavascriptStringObjectEnumerator::JavascriptStringObjectEnumerator(JavascriptStringObject* stringObject,
        ScriptContext* scriptContext,
        BOOL enumNonEnumerable,
        bool enumSymbols) :
        JavascriptEnumerator(scriptContext),
        stringObject(stringObject),
        enumNonEnumerable(enumNonEnumerable),
        enumSymbols(enumSymbols)
    {
        Reset();
    }

    Var JavascriptStringObjectEnumerator::GetCurrentIndex()
    {
        if (stringEnumerator != nullptr)
        {
            return stringEnumerator->GetCurrentIndex();
        }
        else if (objectEnumerator != nullptr)
        {
            return objectEnumerator->GetCurrentIndex();
        }
        else
        {
            return GetLibrary()->GetUndefined();
        }
    }

    Var JavascriptStringObjectEnumerator::GetCurrentValue()
    {
        if (stringEnumerator != nullptr)
        {
            return stringEnumerator->GetCurrentValue();
        }
        else if (objectEnumerator != nullptr)
        {
            return objectEnumerator->GetCurrentValue();
        }
        else
        {
            return GetLibrary()->GetUndefined();
        }
    }

    BOOL JavascriptStringObjectEnumerator::MoveNext(PropertyAttributes* attributes)
    {
        if (stringEnumerator != nullptr)
        {
            if (stringEnumerator->MoveNext(attributes))
            {
                return true;
            }
            stringEnumerator = nullptr;
        }
        if (objectEnumerator != nullptr)
        {
            if (objectEnumerator->MoveNext(attributes))
            {
                return true;
            }
            objectEnumerator = nullptr;
        }
        return false;
    }

    bool JavascriptStringObjectEnumerator::GetCurrentPropertyId(PropertyId* propertyId)
    {
        if (stringEnumerator != nullptr)
        {
            *propertyId = Constants::NoProperty;
            return false;
        }
        if (objectEnumerator != nullptr)
        {
            return objectEnumerator->GetCurrentPropertyId(propertyId);
        }
        *propertyId = Constants::NoProperty;
        return false;
    }

    Var JavascriptStringObjectEnumerator::GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {
        Var currentIndex;
        if (stringEnumerator != nullptr)
        {
            currentIndex = stringEnumerator->GetCurrentAndMoveNext(propertyId, attributes);
            if (currentIndex != nullptr)
            {
                return currentIndex;
            }
            stringEnumerator = nullptr;
        }
        if (objectEnumerator != nullptr)
        {
            currentIndex = objectEnumerator->GetCurrentAndMoveNext(propertyId, attributes);
            if (currentIndex != nullptr)
            {
                return currentIndex;
            }
            objectEnumerator = nullptr;
        }
        return nullptr;
    }


    void JavascriptStringObjectEnumerator::Reset()
    {
        ScriptContext* scriptContext = GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();
        stringEnumerator = RecyclerNew(recycler, JavascriptStringEnumerator, JavascriptString::FromVar(CrossSite::MarshalVar(scriptContext, stringObject->Unwrap())), scriptContext);
        Var enumerator;
        stringObject->DynamicObject::GetEnumerator(enumNonEnumerable, &enumerator, scriptContext, true, enumSymbols);
        objectEnumerator = (JavascriptEnumerator*)enumerator;
    }
}

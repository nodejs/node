//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
namespace Js
{
    JavascriptRegExpEnumerator::JavascriptRegExpEnumerator(JavascriptRegExpConstructor* regExpObject, ScriptContext * requestContext, BOOL enumNonEnumerable) :
        JavascriptEnumerator(requestContext),
        regExpObject(regExpObject),
        enumNonEnumerable(enumNonEnumerable)
    {
        index = (uint)-1;
    }

    Var JavascriptRegExpEnumerator::GetCurrentIndex()
    {
        ScriptContext *scriptContext = regExpObject->GetScriptContext();

        if (index != (uint)-1 && index < regExpObject->GetSpecialEnumerablePropertyCount())
        {
            return scriptContext->GetIntegerString(index);
        }
        else
        {
            return scriptContext->GetLibrary()->GetUndefined();
        }
    }

    Var JavascriptRegExpEnumerator::GetCurrentValue()
    {
        ScriptContext* scriptContext = regExpObject->GetScriptContext();

        if (index != -1 && index < regExpObject->GetSpecialEnumerablePropertyCount())
        {
            Var item;
            if (regExpObject->GetSpecialEnumerablePropertyName(index, &item, scriptContext))
            {
                return item;
            }
        }

        return scriptContext->GetLibrary()->GetUndefined();
    }

    BOOL JavascriptRegExpEnumerator::MoveNext(PropertyAttributes* attributes)
    {
        if (++index < regExpObject->GetSpecialEnumerablePropertyCount())
        {
            if (attributes != nullptr)
            {
                *attributes = PropertyEnumerable;
            }

            return true;
        }
        else
        {
            index = regExpObject->GetSpecialEnumerablePropertyCount();
            return false;
        }
    }

    void JavascriptRegExpEnumerator::Reset()
    {
        index = (uint)-1;
    }

    Var JavascriptRegExpEnumerator::GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {
        propertyId = Constants::NoProperty;
        ScriptContext* scriptContext = regExpObject->GetScriptContext();
        if (++index < regExpObject->GetSpecialEnumerablePropertyCount())
        {
            if (attributes != nullptr)
            {
                *attributes = PropertyEnumerable;
            }

            Var item;
            if (regExpObject->GetSpecialEnumerablePropertyName(index, &item, scriptContext))
            {
                return item;
            }
            return regExpObject->GetScriptContext()->GetIntegerString(index);
        }
        else
        {
            index = regExpObject->GetSpecialEnumerablePropertyCount();
            return nullptr;
        }
    }

    JavascriptRegExpObjectEnumerator::JavascriptRegExpObjectEnumerator(JavascriptRegExpConstructor* regExpObject,
        ScriptContext* scriptContext, BOOL enumNonEnumerable, bool enumSymbols) :
        JavascriptEnumerator(scriptContext),
        regExpObject(regExpObject),
        enumNonEnumerable(enumNonEnumerable),
        enumSymbols(enumSymbols)
    {
        Reset();
    }

    Var JavascriptRegExpObjectEnumerator::GetCurrentIndex()
    {
        if (regExpEnumerator != nullptr)
        {
            return regExpEnumerator->GetCurrentIndex();
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

    Var JavascriptRegExpObjectEnumerator::GetCurrentValue()
    {
        if (regExpEnumerator != nullptr)
        {
            return regExpEnumerator->GetCurrentValue();
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

    BOOL JavascriptRegExpObjectEnumerator::MoveNext(PropertyAttributes* attributes)
    {
        if (regExpEnumerator != nullptr)
        {
            if (regExpEnumerator->MoveNext(attributes))
            {
                return true;
            }
            regExpEnumerator = nullptr;
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

    bool JavascriptRegExpObjectEnumerator::GetCurrentPropertyId(PropertyId *pPropertyId)
    {
        if (regExpEnumerator != nullptr)
        {
            *pPropertyId = Constants::NoProperty;
            return false;
        }

        if (objectEnumerator != nullptr)
        {
            return objectEnumerator->GetCurrentPropertyId(pPropertyId);
        }

        *pPropertyId = Constants::NoProperty;
        return false;
    }

    Var JavascriptRegExpObjectEnumerator::GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {
        Var currentIndex;
        if (regExpEnumerator != nullptr)
        {
            currentIndex = regExpEnumerator->GetCurrentAndMoveNext(propertyId, attributes);
            if (currentIndex != nullptr)
            {
                return currentIndex;
            }
            regExpEnumerator = nullptr;
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

    void JavascriptRegExpObjectEnumerator::Reset()
    {
        ScriptContext* scriptContext = GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();
        regExpEnumerator = RecyclerNew(recycler, JavascriptRegExpEnumerator, regExpObject, scriptContext, enumNonEnumerable);
        Var enumerator;
        regExpObject->DynamicObject::GetEnumerator(enumNonEnumerable, &enumerator, scriptContext, false, enumSymbols);
        objectEnumerator = (JavascriptEnumerator*)enumerator;
    }
}

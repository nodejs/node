//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics>
    JavascriptEnumerator* DynamicObjectEnumerator<T, enumNonEnumerable, enumSymbols, snapShotSementics>::New(ScriptContext* scriptContext, DynamicObject* object)
    {
        DynamicObjectEnumerator* enumerator = RecyclerNew(scriptContext->GetRecycler(), DynamicObjectEnumerator, scriptContext);
        enumerator->Initialize(object);
        return enumerator;
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics>
    DynamicType *DynamicObjectEnumerator<T, enumNonEnumerable, enumSymbols, snapShotSementics>::GetTypeToEnumerate() const
    {
        return
            snapShotSementics &&
            initialType->GetIsLocked() &&
            CONFIG_FLAG(TypeSnapshotEnumeration)
                ? initialType
                : object->GetDynamicType();
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics>
    Var DynamicObjectEnumerator<T, enumNonEnumerable, enumSymbols, snapShotSementics>::GetCurrentIndex()
    {
        if (arrayEnumerator)
        {
            return arrayEnumerator->GetCurrentIndex();
        }

        JavascriptString* propertyString = nullptr;
        PropertyId propertyId = Constants::NoProperty;
        if (!object->FindNextProperty(objectIndex, &propertyString, &propertyId, nullptr, GetTypeToEnumerate(), !enumNonEnumerable, enumSymbols))
        {
            return this->GetLibrary()->GetUndefined();
        }

        Assert(propertyId == Constants::NoProperty || !Js::IsInternalPropertyId(propertyId));

        return propertyString;
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics>
    Var DynamicObjectEnumerator<T, enumNonEnumerable, enumSymbols, snapShotSementics>::GetCurrentValue()
    {
        if (arrayEnumerator)
        {
            return arrayEnumerator->GetCurrentValue();
        }

        return object->GetNextProperty(objectIndex, GetTypeToEnumerate(), !enumNonEnumerable, enumSymbols);
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics>
    BOOL DynamicObjectEnumerator<T, enumNonEnumerable, enumSymbols, snapShotSementics>::MoveNext(PropertyAttributes* attributes)
    {
        PropertyId propId;
        return GetCurrentAndMoveNext(propId, attributes) != NULL;
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics>
    bool DynamicObjectEnumerator<T, enumNonEnumerable, enumSymbols, snapShotSementics>::GetCurrentPropertyId(PropertyId *pPropertyId)
    {
        if (arrayEnumerator)
        {
            return arrayEnumerator->GetCurrentPropertyId(pPropertyId);
        }
        Js::PropertyId propertyId = object->GetPropertyId((T) objectIndex);

        if ((enumNonEnumerable || (propertyId != Constants::NoProperty && object->IsEnumerable(propertyId))))
        {
            *pPropertyId = propertyId;
            return true;
        }
        else
        {
            return false;
        }
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics>
    uint32 DynamicObjectEnumerator<T, enumNonEnumerable, enumSymbols, snapShotSementics>::GetCurrentItemIndex()
    {
        if (arrayEnumerator)
        {
            return arrayEnumerator->GetCurrentItemIndex();
        }
        else
        {
            return JavascriptArray::InvalidIndex;
        }
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics>
    void DynamicObjectEnumerator<T, enumNonEnumerable, enumSymbols, snapShotSementics>::Reset()
    {
        ResetHelper();
    }

    // Initialize (or reuse) this enumerator for a given object.
    template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics>
    void DynamicObjectEnumerator<T, enumNonEnumerable, enumSymbols, snapShotSementics>::Initialize(DynamicObject* object)
    {
        this->object = object;
        ResetHelper();
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics>
    Var DynamicObjectEnumerator<T, enumNonEnumerable, enumSymbols, snapShotSementics>::GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {
        if (arrayEnumerator)
        {
            Var currentIndex = arrayEnumerator->GetCurrentAndMoveNext(propertyId, attributes);
            if(currentIndex != NULL)
            {
                return currentIndex;
            }
            arrayEnumerator = NULL;
        }

        JavascriptString* propertyString;

        do
        {
            objectIndex++;
            propertyString = nullptr;
            if (!object->FindNextProperty(objectIndex, &propertyString, &propertyId, attributes, GetTypeToEnumerate(), !enumNonEnumerable, enumSymbols))
            {
                // No more properties
                objectIndex--;
                break;
            }
        }
        while (Js::IsInternalPropertyId(propertyId));

        return propertyString;
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics>
    void DynamicObjectEnumerator<T, enumNonEnumerable, enumSymbols, snapShotSementics>::ResetHelper()
    {
        if (object->HasObjectArray())
        {
            // Pass "object" as originalInstance to objectArray enumerator
            BOOL result = object->GetObjectArrayOrFlagsAsArray()->GetEnumerator(object, enumNonEnumerable, (Var*)&arrayEnumerator, GetScriptContext(), snapShotSementics, enumSymbols);
            Assert(result);
        }
        else
        {
            arrayEnumerator = nullptr;
        }
        initialType = object->GetDynamicType();
        objectIndex = (T)-1; // This is Constants::NoSlot or Constants::NoBigSlot
    }

    template class DynamicObjectEnumerator<PropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/true, /*snapShotSementics*/false>;
    template class DynamicObjectEnumerator<BigPropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/true, /*snapShotSementics*/false>;
    template class DynamicObjectEnumerator<PropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/true, /*snapShotSementics*/false>;
    template class DynamicObjectEnumerator<BigPropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/true, /*snapShotSementics*/false>;
    template class DynamicObjectEnumerator<PropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/true, /*snapShotSementics*/true>;
    template class DynamicObjectEnumerator<BigPropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/true, /*snapShotSementics*/true>;
    template class DynamicObjectEnumerator<PropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/false, /*snapShotSementics*/false>;
    template class DynamicObjectEnumerator<BigPropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/false, /*snapShotSementics*/false>;
    template class DynamicObjectEnumerator<PropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/false, /*snapShotSementics*/false>;
    template class DynamicObjectEnumerator<BigPropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/false, /*snapShotSementics*/false>;
    template class DynamicObjectEnumerator<PropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/false, /*snapShotSementics*/true>;
    template class DynamicObjectEnumerator<BigPropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/false, /*snapShotSementics*/true>;
    template class DynamicObjectEnumerator<PropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/false, /*snapShotSementics*/true>;
    template class DynamicObjectEnumerator<BigPropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/false, /*snapShotSementics*/true>;
    template class DynamicObjectEnumerator<PropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/true, /*snapShotSementics*/true>;
    template class DynamicObjectEnumerator<BigPropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/true, /*snapShotSementics*/true>;
}

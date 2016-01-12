//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    template <typename T, bool enumNonEnumerable, bool enumSymbols>
    JavascriptEnumerator* DynamicObjectSnapshotEnumeratorWPCache<T, enumNonEnumerable, enumSymbols>::New(ScriptContext* scriptContext, DynamicObject* object)
    {
        DynamicObjectSnapshotEnumeratorWPCache* enumerator = RecyclerNew(scriptContext->GetRecycler(), DynamicObjectSnapshotEnumeratorWPCache, scriptContext);
        enumerator->Initialize(object, false);
        return enumerator;
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols>
    void DynamicObjectSnapshotEnumeratorWPCache<T, enumNonEnumerable, enumSymbols>::Initialize(DynamicObject* object, bool allowUnlockedType/*= false*/)
    {
        __super::Initialize(object);

        enumeratedCount = 0;

        if (!allowUnlockedType)
        {
            Assert(initialType->GetIsLocked());
        }
        else if (initialType->GetIsLocked())
        {
            VirtualTableInfo<DynamicObjectSnapshotEnumeratorWPCache>::SetVirtualTable(this); // Fix vtable which could have been downgraded
        }
        else
        {
            VirtualTableInfo<DynamicObjectSnapshotEnumerator>::SetVirtualTable(this); // Downgrade to normal snapshot enumerator
            return;
        }

        ScriptContext* scriptContext = this->GetScriptContext();
        ThreadContext * threadContext = scriptContext->GetThreadContext();
        CachedData * data = (CachedData *)threadContext->GetDynamicObjectEnumeratorCache(initialType);

        if (data == nullptr || data->enumNonEnumerable != enumNonEnumerable || data->enumSymbols != enumSymbols)
        {
            data = RecyclerNewStructPlus(scriptContext->GetRecycler(),
                initialPropertyCount * sizeof(PropertyString *) + initialPropertyCount * sizeof(T) + initialPropertyCount * sizeof(PropertyAttributes), CachedData);
            data->cachedCount = 0;
            data->strings = (PropertyString **)(data + 1);
            data->indexes = (T *)(data->strings + initialPropertyCount);
            data->attributes = (PropertyAttributes*)(data->indexes + initialPropertyCount);
            data->completed = false;
            data->enumNonEnumerable = enumNonEnumerable;
            data->enumSymbols = enumSymbols;
            threadContext->AddDynamicObjectEnumeratorCache(initialType, data);
        }
        this->cachedData = data;
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols>
    JavascriptString *
        DynamicObjectSnapshotEnumeratorWPCache<T, enumNonEnumerable, enumSymbols>::GetCurrentAndMoveNextFromObjectWPCache(T& index, PropertyId& propertyId, PropertyAttributes* attributes)
    {
        if (initialType != object->GetDynamicType())
        {
            if (this->IsCrossSiteEnumerator())
            {
                // downgrade back to the normal snapshot enumerator
                VirtualTableInfo<CrossSiteEnumerator<DynamicObjectSnapshotEnumerator>>::SetVirtualTable(this);
            }
            else
            {
                // downgrade back to the normal snapshot enumerator
                VirtualTableInfo<DynamicObjectSnapshotEnumerator>::SetVirtualTable(this);
            }
            return this->GetCurrentAndMoveNextFromObject(objectIndex, propertyId, attributes);
        }
        Assert(enumeratedCount <= cachedData->cachedCount);
        JavascriptString* propertyStringName;
        PropertyAttributes propertyAttributes = PropertyNone;
        if (enumeratedCount < cachedData->cachedCount)
        {
            PropertyString * propertyString = cachedData->strings[enumeratedCount];
            propertyStringName = propertyString;
            propertyId = propertyString->GetPropertyRecord()->GetPropertyId();

#if DBG
            PropertyId tempPropertyId;
            /* JavascriptString * tempPropertyString = */ this->GetCurrentAndMoveNextFromObject(objectIndex, tempPropertyId, attributes);

            Assert(tempPropertyId == propertyId);
            Assert(objectIndex == cachedData->indexes[enumeratedCount]);
#endif
            objectIndex = cachedData->indexes[enumeratedCount];
            propertyAttributes = cachedData->attributes[enumeratedCount];

            enumeratedCount++;
        }
        else if (!cachedData->completed)
        {
            propertyStringName = this->GetCurrentAndMoveNextFromObject(objectIndex, propertyId, &propertyAttributes);

            if (propertyStringName && VirtualTableInfo<PropertyString>::HasVirtualTable(propertyStringName))
            {
                Assert(enumeratedCount < initialPropertyCount);
                cachedData->strings[enumeratedCount] = (PropertyString*)propertyStringName;
                cachedData->indexes[enumeratedCount] = objectIndex;
                cachedData->attributes[enumeratedCount] = propertyAttributes;
                cachedData->cachedCount = ++enumeratedCount;
            }
            else
            {
                cachedData->completed = true;
            }
        }
        else
        {
#if DBG
            PropertyId tempPropertyId;
            Assert(this->GetCurrentAndMoveNextFromObject(objectIndex, tempPropertyId, attributes) == nullptr);
#endif
            propertyStringName = nullptr;
        }

        if (attributes != nullptr)
        {
            *attributes = propertyAttributes;
        }

        return propertyStringName;
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols>
    Var DynamicObjectSnapshotEnumeratorWPCache<T, enumNonEnumerable, enumSymbols>::GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {
        Var currentIndex = GetCurrentAndMoveNextFromArray(propertyId, attributes);

        if (currentIndex == nullptr)
        {
            currentIndex = this->GetCurrentAndMoveNextFromObjectWPCache(objectIndex, propertyId, attributes);
        }

        return currentIndex;
    }

    template <typename T, bool enumNonEnumerable, bool enumSymbols>
    void DynamicObjectSnapshotEnumeratorWPCache<T, enumNonEnumerable, enumSymbols>::Reset()
    {
        // If we are reusing the enumerator the object type should be the same
        Assert(object->GetDynamicType() == initialType);
        Assert(initialPropertyCount == object->GetPropertyCount());

        __super::Reset();

        this->enumeratedCount = 0;
    }

    template class DynamicObjectSnapshotEnumeratorWPCache<PropertyIndex, true, true>;
    template class DynamicObjectSnapshotEnumeratorWPCache<PropertyIndex, true, false>;
    template class DynamicObjectSnapshotEnumeratorWPCache<PropertyIndex, false, true>;
    template class DynamicObjectSnapshotEnumeratorWPCache<PropertyIndex, false, false>;
    template class DynamicObjectSnapshotEnumeratorWPCache<BigPropertyIndex, true, true>;
    template class DynamicObjectSnapshotEnumeratorWPCache<BigPropertyIndex, true, false>;
    template class DynamicObjectSnapshotEnumeratorWPCache<BigPropertyIndex, false, true>;
    template class DynamicObjectSnapshotEnumeratorWPCache<BigPropertyIndex, false, false>;
};

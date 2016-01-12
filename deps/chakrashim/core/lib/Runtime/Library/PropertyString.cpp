//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(PropertyString);
    DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(PropertyString);

    PropertyString::PropertyString(StaticType* type, const Js::PropertyRecord* propertyRecord, bool registerScriptContext)
        : JavascriptString(type, propertyRecord->GetLength(), propertyRecord->GetBuffer()), registerScriptContext(registerScriptContext), m_propertyRecord(propertyRecord)
    {
    }

    PropertyString* PropertyString::New(StaticType* type, const Js::PropertyRecord* propertyRecord, ArenaAllocator *arena)
    {
        PropertyString * propertyString = Anew(arena, PropertyString, type, propertyRecord, true);
        propertyString->propCache = AllocatorNewStructZ(InlineCacheAllocator, type->GetScriptContext()->GetInlineCacheAllocator(), PropertyCache);
        return propertyString;
    }


    PropertyString* PropertyString::New(StaticType* type, const Js::PropertyRecord* propertyRecord, Recycler *recycler)
    {
        PropertyString * propertyString =  RecyclerNewPlusZ(recycler, sizeof(PropertyCache), PropertyString, type, propertyRecord, false);
        propertyString->propCache = (PropertyCache*)(propertyString + 1);
        return propertyString;
    }

    PropertyCache const * PropertyString::GetPropertyCache() const
    {
        Assert(!propCache->type  || propCache->type->GetScriptContext() == this->GetScriptContext());
        return propCache;
    }

    void PropertyString::ClearPropertyCache()
    {
        this->propCache->type = nullptr;
    }
    void const * PropertyString::GetOriginalStringReference()
    {
        // Property record is the allocation containing the string buffer
        return this->m_propertyRecord;
    }

    RecyclableObject * PropertyString::CloneToScriptContext(ScriptContext* requestContext)
    {
        return requestContext->GetLibrary()->CreatePropertyString(this->m_propertyRecord);
    }

    void PropertyString::UpdateCache(Type * type, uint16 dataSlotIndex, bool isInlineSlot, bool isStoreFieldEnabled)
    {
        Assert(type && type->GetScriptContext() == this->GetScriptContext());
        if (registerScriptContext)
        {
            this->GetScriptContext()->RegisterAsScriptContextWithInlineCaches();
        }

        this->propCache->type = type;
        this->propCache->preventdataSlotIndexFalseRef = 1;
        this->propCache->dataSlotIndex = dataSlotIndex;
        this->propCache->preventFlagsFalseRef = 1;
        this->propCache->isInlineSlot = isInlineSlot;
        this->propCache->isStoreFieldEnabled = isStoreFieldEnabled;
    }
}

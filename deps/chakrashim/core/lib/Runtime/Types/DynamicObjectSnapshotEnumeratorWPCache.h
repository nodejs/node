//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    /**************************************************************************************
     * DynamicObjectSnapshotEnumeratorWPCache
     *      This variant of the snapshot enumerator is only used by shared types.
     *      Shared type's enumerator order doesn't change, so we can cache the enumeration
     *      order in a map on the thread context and reuse the same order the next time
     *      the same type is enumerated, thus speeding up enumeration by eliminating
     *      virtual calls, internal property checks, and property string lookup
     **************************************************************************************/
    template <typename T, bool enumNonEnumerable, bool enumSymbols>
    class DynamicObjectSnapshotEnumeratorWPCache : public DynamicObjectSnapshotEnumerator<T, enumNonEnumerable, enumSymbols>
    {
    protected:
        DEFINE_VTABLE_CTOR(DynamicObjectSnapshotEnumeratorWPCache, DynamicObjectSnapshotEnumerator);
        DEFINE_MARSHAL_ENUMERATOR_TO_SCRIPT_CONTEXT(DynamicObjectSnapshotEnumeratorWPCache);

    private:
        DynamicObjectSnapshotEnumeratorWPCache(ScriptContext* scriptContext)
            : DynamicObjectSnapshotEnumerator(scriptContext)
        {
        }

        JavascriptString * GetCurrentAndMoveNextFromObjectWPCache(T& index, PropertyId& propertyId, PropertyAttributes* attributes);

        struct CachedData
        {
            PropertyString ** strings;
            T * indexes;
            PropertyAttributes * attributes;
            int cachedCount;
            bool completed;
            bool enumNonEnumerable;
            bool enumSymbols;
        } * cachedData;

        int enumeratedCount; // Use int type to make fast path cmp easier. Note this works for both small and big PropertyIndex.

        friend class ForInObjectEnumerator;
        DynamicObjectSnapshotEnumeratorWPCache() { /* Do nothing, needed by the vtable ctor for ForInObjectEnumeratorWrapper */ }
        void Initialize(DynamicObject* object, bool allowUnlockedType = false);

    public:
        static JavascriptEnumerator* New(ScriptContext* scriptContext, DynamicObject* object);
        virtual void Reset() override;
        virtual Var GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes = nullptr) override;

        static uint32 GetOffsetOfInitialType() { return offsetof(DynamicObjectSnapshotEnumeratorWPCache, initialType); }
        static uint32 GetOffsetOfEnumeratedCount() { return offsetof(DynamicObjectSnapshotEnumeratorWPCache, enumeratedCount); }
        static uint32 GetOffsetOfCachedData() { return offsetof(DynamicObjectSnapshotEnumeratorWPCache, cachedData); }

        static uint32 GetOffsetOfCachedDataStrings() { return offsetof(CachedData, strings); }
        static uint32 GetOffsetOfCachedDataIndexes() { return offsetof(CachedData, indexes); }
        static uint32 GetOffsetOfCachedDataCachedCount() { return offsetof(CachedData, cachedCount); }
        static uint32 GetOffsetOfCachedDataPropertyAttributes() { return offsetof(CachedData, attributes); }
    };
}

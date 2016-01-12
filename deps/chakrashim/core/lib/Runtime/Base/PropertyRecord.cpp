//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(PropertyRecord);
    DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(PropertyRecord);

    // Constructor for runtime-constructed PropertyRecords
    PropertyRecord::PropertyRecord(DWORD byteCount, bool isNumeric, uint hash, bool isSymbol)
        : pid(Js::Constants::NoProperty), hash(hash), isNumeric(isNumeric), byteCount(byteCount), isBound(false), isSymbol(isSymbol)
    {
    }

    // Constructor for built-in PropertyRecords
    PropertyRecord::PropertyRecord(PropertyId pid, uint hash, bool isNumeric, DWORD byteCount, bool isSymbol)
        : pid(pid), hash(hash), isNumeric(isNumeric), byteCount(byteCount), isBound(true), isSymbol(isSymbol)
    {
    }

    void PropertyRecord::Finalize(bool isShutdown)
    {
        if (!isShutdown)
        {
            ThreadContext * tc = ThreadContext::GetContextForCurrentThread();
            Assert(tc);
            Assert(tc->IsActivePropertyId(this->GetPropertyId()));

            tc->InvalidatePropertyRecord(this);
        }
    }

#ifdef DEBUG
    // This is only used to assert that integer property names are not passed into
    // the GetSetter, GetProperty, SetProperty etc methods that take JavascriptString
    // instead of PropertyId.  It is expected that integer property names will go
    // through a fast path before reaching those APIs.
    bool PropertyRecord::IsPropertyNameNumeric(const wchar_t* str, int length)
    {
        uint32 unused;
        return IsPropertyNameNumeric(str, length, &unused);
    }
#endif

    bool PropertyRecord::IsPropertyNameNumeric(const wchar_t* str, int length, uint32* intVal)
    {
        return (Js::JavascriptOperators::TryConvertToUInt32(str, length, intVal) &&
            (*intVal != Js::JavascriptArray::InvalidIndex));
    }

    uint32 PropertyRecord::GetNumericValue() const
    {
        Assert(IsNumeric());
        return *(uint32 *)(this->GetBuffer() + this->GetLength() + 1);
    }

    // Initialize all Internal property records
#define INTERNALPROPERTY(name) \
    const BuiltInPropertyRecord<1> InternalPropertyRecords::name = { PropertyRecord((PropertyId)InternalPropertyIds::name, (uint)InternalPropertyIds::name, false, 0, false), L"" };
#include "..\InternalPropertyList.h"

    const PropertyRecord* InternalPropertyRecords::GetInternalPropertyName(PropertyId propertyId)
    {
        Assert(IsInternalPropertyId(propertyId));

        switch (propertyId)
        {
#define INTERNALPROPERTY(name) \
            case InternalPropertyIds::name: \
                return InternalPropertyRecords::name;
#include "..\InternalPropertyList.h"
        }

        Throw::FatalInternalError();
    }


    PropertyAttributes PropertyRecord::DefaultAttributesForPropertyId(PropertyId propertyId, bool __proto__AsDeleted)
    {
        switch (propertyId)
        {
        case PropertyIds::__proto__:
            if (__proto__AsDeleted)
            {
                //
                // If the property name is __proto__, it could be either [[prototype]] or ignored, or become a local
                // property depending on later environment and property value. To maintain enumeration order when it
                // becomes a local property, add the entry as deleted.
                //
                return PropertyDeletedDefaults;
            }
            return PropertyDynamicTypeDefaults;

        default:
            return PropertyDynamicTypeDefaults;
        }
    }

    // Initialize all BuiltIn property records
    const BuiltInPropertyRecord<1> BuiltInPropertyRecords::EMPTY = { PropertyRecord(PropertyIds::_none, 0, false, 0, false), L"" };
#define ENTRY_INTERNAL_SYMBOL(n) const BuiltInPropertyRecord<ARRAYSIZE(L"<" L#n L">")> BuiltInPropertyRecords::n = { PropertyRecord(PropertyIds::n, (uint)PropertyIds::n, false, (ARRAYSIZE(L"<" L#n L">") - 1) * sizeof(wchar_t), true), L"<" L#n L">" };
#define ENTRY_SYMBOL(n, d) const BuiltInPropertyRecord<ARRAYSIZE(d)> BuiltInPropertyRecords::n = { PropertyRecord(PropertyIds::n, 0, false, (ARRAYSIZE(d) - 1) * sizeof(wchar_t), true), d };
#define ENTRY2(n, s) const BuiltInPropertyRecord<ARRAYSIZE(s)> BuiltInPropertyRecords::n = { PropertyRecord(PropertyIds::n, 0, false, (ARRAYSIZE(s) - 1) * sizeof(wchar_t), false), s };
#define ENTRY(n) ENTRY2(n, L#n)
#include "Base\JnDirectFields.h"
};

namespace JsUtil
{
    bool NoCaseComparer<Js::CaseInvariantPropertyListWithHashCode*>::Equals(_In_ Js::CaseInvariantPropertyListWithHashCode* list1, JsUtil::CharacterBuffer<WCHAR> const& str)
    {
        Assert(list1 != nullptr);

        const RecyclerWeakReference<Js::PropertyRecord const>* propRecordWeakRef = list1->CompactEnd<true>();

        // If the lists are empty post-compaction, thats fine, we'll just remove them later
        if (propRecordWeakRef != nullptr)
        {
            const Js::PropertyRecord* prop = propRecordWeakRef->Get();

            // Since compaction returned this pointer, their strong refs should not be null
            Assert(prop);

            JsUtil::CharacterBuffer<WCHAR> string(prop->GetBuffer(), prop->GetLength());

            return NoCaseComparer<JsUtil::CharacterBuffer<WCHAR> >::Equals(string, str);
        }

        // If either of the property strings contains no entries, the two lists are not equivalent
        return false;
    }

    bool NoCaseComparer<Js::CaseInvariantPropertyListWithHashCode*>::Equals(_In_ Js::CaseInvariantPropertyListWithHashCode* list1, _In_ Js::CaseInvariantPropertyListWithHashCode* list2)
    {
        Assert(list1 != nullptr && list2 != nullptr);

        // If the two lists are the same, they're equal
        if (list1 == list2)
        {
            return true;
        }

        // If they don't have the same case invariant hash code, they're not equal
        if (list1->caseInvariantHashCode != list2->caseInvariantHashCode)
        {
            return false;
        }

        // Find a string from list 2
        // If it's the same when compared with a string from list 1 in a case insensitive way, they're equal
        const RecyclerWeakReference<Js::PropertyRecord const>* propRecordWeakRef = list2->CompactEnd<true>();

        if (propRecordWeakRef != nullptr)
        {
            const Js::PropertyRecord* prop = propRecordWeakRef->Get();

            // Since compaction returned this pointer, their strong refs should not be null
            Assert(prop);

            JsUtil::CharacterBuffer<WCHAR> string(prop->GetBuffer(), prop->GetLength());

            return NoCaseComparer<Js::CaseInvariantPropertyListWithHashCode*>::Equals(list1, string);
        }

        return false;
    }

    uint NoCaseComparer<Js::CaseInvariantPropertyListWithHashCode*>::GetHashCode(_In_ Js::CaseInvariantPropertyListWithHashCode* list)
    {
        Assert(list != nullptr);

        if (list->caseInvariantHashCode == 0)
        {
            const RecyclerWeakReference<Js::PropertyRecord const>* propRecordWeakRef = list->CompactEnd<true>();

            if (propRecordWeakRef != nullptr)
            {
                const Js::PropertyRecord* prop = propRecordWeakRef->Get();

                Assert(prop);

                JsUtil::CharacterBuffer<WCHAR> string(prop->GetBuffer(), prop->GetLength());

                list->caseInvariantHashCode = NoCaseComparer<JsUtil::CharacterBuffer<WCHAR> >::GetHashCode(string);
            }
        }

        return list->caseInvariantHashCode;
    }
}

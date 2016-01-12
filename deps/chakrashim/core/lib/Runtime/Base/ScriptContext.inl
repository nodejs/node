//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    inline PropertyString* ScriptContext::GetPropertyString2(wchar_t ch1, wchar_t ch2) {
        if (ch1 < '0' || ch1 > 'z' || ch2 < '0' || ch2 > 'z')
            return NULL;
        const uint i=PropertyStringMap::PStrMapIndex(ch1);
        if (propertyStrings[i]==NULL)
            return NULL;
        const uint j=PropertyStringMap::PStrMapIndex(ch2);
        return propertyStrings[i]->strLen2[j];
    }

    inline void ScriptContext::FindPropertyRecord(JavascriptString *pstName, PropertyRecord const ** propertyRecord)
    {
        threadContext->FindPropertyRecord(pstName, propertyRecord);
    }

    inline void ScriptContext::FindPropertyRecord(__in LPCWSTR propertyName, __in int propertyNameLength, PropertyRecord const ** propertyRecord)
    {
        threadContext->FindPropertyRecord(propertyName, propertyNameLength, propertyRecord);
    }

    inline JsUtil::List<const RecyclerWeakReference<Js::PropertyRecord const>*>* ScriptContext::FindPropertyIdNoCase(__in LPCWSTR propertyName,__in int propertyNameLength)
    {
        return threadContext->FindPropertyIdNoCase(this, propertyName, propertyNameLength);
    }

    inline PropertyId ScriptContext::GetOrAddPropertyIdTracked(JsUtil::CharacterBuffer<WCHAR> const& propName)
    {
        Js::PropertyRecord const * propertyRecord;
        threadContext->GetOrAddPropertyId(propName, &propertyRecord);

        this->TrackPid(propertyRecord);

        return propertyRecord->GetPropertyId();
    }

    template <size_t N>
    inline PropertyId ScriptContext::GetOrAddPropertyIdTracked(const wchar_t(&propertyName)[N])
    {
        return GetOrAddPropertyIdTracked(propertyName, N - 1);
    }

    inline void ScriptContext::GetOrAddPropertyRecord(JsUtil::CharacterBuffer<WCHAR> const& propertyName, PropertyRecord const ** propertyRecord)
    {
        threadContext->GetOrAddPropertyId(propertyName, propertyRecord);
    }

    template <size_t N>
    inline void ScriptContext::GetOrAddPropertyRecord(const wchar_t(&propertyName)[N], PropertyRecord const** propertyRecord)
    {
        GetOrAddPropertyRecord(propertyName, N - 1, propertyRecord);
    }

    inline PropertyId ScriptContext::GetOrAddPropertyIdTracked(__in_ecount(propertyNameLength) LPCWSTR propertyName, __in int propertyNameLength)
    {
        Js::PropertyRecord const * propertyRecord;
        threadContext->GetOrAddPropertyId(propertyName, propertyNameLength, &propertyRecord);
        if (propertyNameLength == 2)
        {
            CachePropertyString2(propertyRecord);
        }
        this->TrackPid(propertyRecord);

        return propertyRecord->GetPropertyId();
    }

    inline void ScriptContext::GetOrAddPropertyRecord(__in_ecount(propertyNameLength) LPCWSTR propertyName, __in int propertyNameLength, PropertyRecord const ** propertyRecord)
    {
        threadContext->GetOrAddPropertyId(propertyName, propertyNameLength, propertyRecord);
        if (propertyNameLength == 2)
        {
            CachePropertyString2(*propertyRecord);
        }
    }

    template <typename TCacheType>
    void ScriptContext::CleanDynamicFunctionCache(TCacheType* cacheType)
    {
        // Remove eval map functions that haven't been recently used
        cacheType->Clean([this](const TCacheType::KeyType& key, TCacheType::ValueType value) {
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            if (CONFIG_FLAG(DumpEvalStringOnRemoval))
            {
                Output::Print(L"EvalMap: Removing Dynamic Function String from dynamic function cache: %s\n", key.str.GetBuffer()); Output::Flush();
            }
#endif
        });
    }

    template <class TDelegate>
    void ScriptContext::MapFunction(TDelegate mapper)
    {
        if (this->sourceList)
        {
            this->sourceList->Map([&mapper] (int, RecyclerWeakReference<Js::Utf8SourceInfo>* sourceInfo)
            {
                Utf8SourceInfo* sourceInfoStrongRef = sourceInfo->Get();
                if (sourceInfoStrongRef)
                {
                    sourceInfoStrongRef->MapFunction(mapper);
                }
            });
        }
    }

    template <class TDelegate>
    FunctionBody* ScriptContext::FindFunction(TDelegate predicate)
    {
        FunctionBody* functionBody = nullptr;

        this->sourceList->MapUntil([&functionBody, &predicate] (int, RecyclerWeakReference<Js::Utf8SourceInfo>* sourceInfo) -> bool
        {
            Utf8SourceInfo* sourceInfoStrongRef = sourceInfo->Get();
            if (sourceInfoStrongRef)
            {
                functionBody = sourceInfoStrongRef->FindFunction(predicate);
                if (functionBody)
                {
                    return true;
                }
            }

            return false;
        });

        return functionBody;
    }

    inline BOOL ScriptContext::IsNumericPropertyId(PropertyId propertyId, uint32* value)
    {
        BOOL isNumericPropertyId = threadContext->IsNumericPropertyId(propertyId, value);

#if DEBUG
        PropertyRecord const * name = this->GetPropertyName(propertyId);

        if (name != nullptr)
        {
            // Symbol properties are not numeric - description should not be used.
            if (name->IsSymbol())
            {
                return false;
            }

            ulong index;
            BOOL isIndex = JavascriptArray::GetIndex(name->GetBuffer(), &index);
            if (isNumericPropertyId != isIndex)
            {
                // WOOB 1137798: JavascriptArray::GetIndex does not handle embeded NULLs. So if we have a property
                // name "1234\0", JavascriptArray::GetIndex would incorrectly accepts it as an array index property
                // name.
                Assert((size_t)(name->GetLength()) != wcslen(name->GetBuffer()));
            }
            else if (isNumericPropertyId)
            {
                Assert((ulong)*value == index);
            }
        }
#endif

        return isNumericPropertyId;
    }

    inline void ScriptContext::RegisterWeakReferenceDictionary(JsUtil::IWeakReferenceDictionary* weakReferenceDictionary)
    {
        this->weakReferenceDictionaryList.Prepend(this->GeneralAllocator(), weakReferenceDictionary);
    }

    __inline RecyclableObject *ScriptContext::GetMissingPropertyResult(Js::RecyclableObject *instance, Js::PropertyId id)
    {
        return GetLibrary()->GetUndefined();
    }

    __inline RecyclableObject *ScriptContext::GetMissingItemResult(Js::RecyclableObject *instance, uint32 index)
    {
        return GetLibrary()->GetUndefined();
    }

    __inline RecyclableObject *ScriptContext::GetMissingParameterValue(Js::JavascriptFunction *function, uint32 paramIndex)
    {
        return GetLibrary()->GetUndefined();
    }

    __inline RecyclableObject *ScriptContext::GetNullPropertyResult(Js::RecyclableObject *instance, Js::PropertyId id)
    {
        return GetLibrary()->GetNull();
    }

    __inline RecyclableObject *ScriptContext::GetNullItemResult(Js::RecyclableObject *instance, uint32 index)
    {
        return GetLibrary()->GetUndefined();
    }

    __inline SRCINFO *ScriptContext::AddHostSrcInfo(SRCINFO const *pSrcInfo)
    {
        Assert(pSrcInfo != nullptr);

        return RecyclerNewZ(this->GetRecycler(), SRCINFO, *pSrcInfo);
    }
}

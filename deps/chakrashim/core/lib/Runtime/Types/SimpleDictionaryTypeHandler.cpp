//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

#include "Library\ForInObjectEnumerator.h"

namespace Js
{
    // ----------------------------------------------------------------------
    // Helper methods to deal with differing TMapKey and TPropertyKey types.
    // Used by both SimpleDictionaryTypeHandler and DictionaryTypeHandler.
    // ----------------------------------------------------------------------

    PropertyId TMapKey_GetPropertyId(ScriptContext* scriptContext, const PropertyRecord* key)
    {
        return key->GetPropertyId();
    }

    PropertyId TMapKey_GetPropertyId(ScriptContext* scriptContext, JavascriptString* key)
    {
        return scriptContext->GetOrAddPropertyIdTracked(key->GetSz(), key->GetLength());
    }

    bool TMapKey_IsSymbol(const PropertyRecord* key, ScriptContext* scriptContext)
    {
        return key->IsSymbol();
    }

    bool TMapKey_IsSymbol(JavascriptString* key, ScriptContext* scriptContext)
    {
        // Property indexed via string cannot be a symbol.
        return false;
    }

    bool TMapKey_IsSymbol(PropertyId key, ScriptContext* scriptContext)
    {
        return scriptContext->GetPropertyName(key)->IsSymbol();
    }

    template<typename TMapKey>
    TMapKey TMapKey_ConvertKey(ScriptContext* scriptContext, const PropertyRecord* key)
    {
        return key;
    }

    template<>
    JavascriptString* TMapKey_ConvertKey(ScriptContext* scriptContext, const PropertyRecord* key)
    {
        // String keyed type handlers can't handle InternalPropertyIds because they have no string representation
        // so assert that no code paths convert InternalPropertyIds to PropertyStrings.
        Assert(!IsInternalPropertyId(key->GetPropertyId()));
        // The same is true for symbols - we should not be converting a symbol property into a PropertyString.
        Assert(!key->IsSymbol());

        return scriptContext->GetPropertyString(key->GetPropertyId());
    }

    template<typename TMapKey>
    TMapKey TMapKey_ConvertKey(ScriptContext* scriptContext, JavascriptString* key)
    {
        return key;
    }

    template<>
    const PropertyRecord* TMapKey_ConvertKey(ScriptContext* scriptContext, JavascriptString* key)
    {
        PropertyRecord const * propertyRecord;
        if (VirtualTableInfo<Js::PropertyString>::HasVirtualTable(key))
        {
            propertyRecord = ((PropertyString*)key)->GetPropertyRecord();
        }
        else
        {
            scriptContext->GetOrAddPropertyRecord(key->GetString(), key->GetLength(), &propertyRecord);
        }
        return propertyRecord;
    }

    bool TPropertyKey_IsInternalPropertyId(JavascriptString* key)
    {
        // WARNING: This will return false for PropertyStrings that are actually InternalPropertyIds
        Assert(!VirtualTableInfo<PropertyString>::HasVirtualTable(key) || !IsInternalPropertyId(((PropertyString*)key)->GetPropertyRecord()->GetPropertyId()));
        return false;
    }

    bool TPropertyKey_IsInternalPropertyId(const PropertyRecord* key)
    {
        return IsInternalPropertyId(key->GetPropertyId()) ? true : false;
    }

    bool TPropertyKey_IsInternalPropertyId(PropertyId key)
    {
        return IsInternalPropertyId(key) ? true : false;
    }

    template <typename TMapKey>
    bool TMapKey_IsJavascriptString()
    {
        return false;
    }

    template <>
    bool TMapKey_IsJavascriptString<JavascriptString*>()
    {
        return true;
    }

    template <typename TPropertyKey>
    bool TPropertyKey_IsJavascriptString()
    {
        return false;
    }

    template<>
    bool TPropertyKey_IsJavascriptString<JavascriptString*>()
    {
        return true;
    }

    template <typename TPropertyKey>
    PropertyId TPropertyKey_GetOptionalPropertyId(ScriptContext* scriptContext, TPropertyKey key)
    {
        return key;
    }

    template <>
    PropertyId TPropertyKey_GetOptionalPropertyId(ScriptContext* scriptContext, const PropertyRecord* key)
    {
        return key->GetPropertyId();
    }

    template <>
    PropertyId TPropertyKey_GetOptionalPropertyId(ScriptContext* scriptContext, JavascriptString* key)
    {
        const PropertyRecord* propertyRecord = nullptr;
        scriptContext->FindPropertyRecord(key, &propertyRecord);
        return propertyRecord != nullptr ? propertyRecord->GetPropertyId() : Constants::NoProperty;
    }

    JavascriptString* TMapKey_OptionalConvertPropertyIdToPropertyRecord(ScriptContext* scriptContext, JavascriptString* propertyString)
    {
        return propertyString;
    }

    const PropertyRecord* TMapKey_OptionalConvertPropertyIdToPropertyRecord(ScriptContext* scriptContext, PropertyId propertyId)
    {
        return scriptContext->GetPropertyName(propertyId);
    }


    template <typename TPropertyKey>
    PropertyId TPropertyKey_GetUpdateSideEffectPropertyId(PropertyId propertyId, TPropertyKey propertyKey);

    template <>
    PropertyId TPropertyKey_GetUpdateSideEffectPropertyId<PropertyId>(PropertyId propertyId, PropertyId propertyKey)
    {
        Assert(propertyId != Js::Constants::NoProperty);
        Assert(propertyId == propertyKey);
        return propertyKey;
    }

    template <>
    PropertyId TPropertyKey_GetUpdateSideEffectPropertyId<JavascriptString *>(PropertyId propertyId, JavascriptString * propertyKey)
    {
        if (propertyId != Js::Constants::NoProperty)
        {
            return propertyId;
        }
        JsUtil::CharacterBuffer<WCHAR> propertyStr(propertyKey->GetString(), propertyKey->GetLength());
        if (BuiltInPropertyRecords::valueOf.Equals(propertyStr))
        {
            return PropertyIds::valueOf;
        }
        if (BuiltInPropertyRecords::toString.Equals(propertyStr))
        {
           return PropertyIds::toString;
        }
        return Js::Constants::NoProperty;
    }

#if DBG
    template <typename TPropertyKey>
    bool TPropertyKey_IsNumeric(TPropertyKey key)
    {
        return false;
    }

    template <>
    bool TPropertyKey_IsNumeric(const PropertyRecord* key)
    {
        return key->IsNumeric();
    }
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    const wchar_t* TMapKey_GetBuffer(const PropertyRecord* key)
    {
        return key->GetBuffer();
    }

    const wchar_t* TMapKey_GetBuffer(JavascriptString* key)
    {
        return key->GetSz();
    }
#endif

    // Round up requested property capacity and cap by max range value.
    template <typename Ranges>
    void PropertyIndexRangesBase<Ranges>::VerifySlotCapacity(int requestedCapacity)
    {
        Assert(requestedCapacity <= static_cast<int>(Ranges::MaxValue)); // Should never request more than max range value
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported> * SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::New(Recycler * recycler, int initialCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked, bool isShared)
    {
        PropertyIndexRangesType::VerifySlotCapacity(initialCapacity);
        return RecyclerNew(recycler, SimpleDictionaryTypeHandlerBase, recycler, initialCapacity, inlineSlotCapacity, offsetOfInlineSlots, isLocked, isShared);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported> * SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::New(ScriptContext * scriptContext, SimplePropertyDescriptor const* propertyDescriptors, int propertyCount, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked, bool isShared)
    {
        PropertyIndexRangesType::VerifySlotCapacity(propertyCount);
        return RecyclerNew(scriptContext->GetRecycler(), SimpleDictionaryTypeHandlerBase, scriptContext, propertyDescriptors, propertyCount, propertyCount, inlineSlotCapacity, offsetOfInlineSlots, isLocked, isShared);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SimpleDictionaryTypeHandlerBase(Recycler* recycler) :
        // We can do slotCapacity roundup here because this constructor is always creating type handler for a new object.
        DynamicTypeHandler(1),
        nextPropertyIndex(0),
        singletonInstance(nullptr),
        isUnordered(false),
        numDeletedProperties(0)
    {
        SetIsInlineSlotCapacityLocked();
        propertyMap = RecyclerNew(recycler, SimplePropertyDescriptorMap, recycler, this->GetSlotCapacity());
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SimpleDictionaryTypeHandlerBase(ScriptContext * scriptContext, SimplePropertyDescriptor const* propertyDescriptors, int propertyCount, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked, bool isShared) :
        // Do not RoundUp passed in slotCapacity. This may be called by ConvertTypeHandler for an existing DynamicObject and should use the real existing slotCapacity.
        DynamicTypeHandler(slotCapacity, inlineSlotCapacity, offsetOfInlineSlots, DefaultFlags | (isLocked ? IsLockedFlag : 0) | (isShared ? (MayBecomeSharedFlag | IsSharedFlag) : 0)),
        nextPropertyIndex(0),
        singletonInstance(nullptr),
        isUnordered(false),
        hasNamelessPropertyId(false),
        numDeletedProperties(0)
    {
        SetIsInlineSlotCapacityLocked();
        Assert(slotCapacity <= MaxPropertyIndexSize);
        propertyMap = RecyclerNew(scriptContext->GetRecycler(), SimplePropertyDescriptorMap, scriptContext->GetRecycler(), propertyCount);

        for (int i=0; i < propertyCount; i++)
        {
            Add(propertyDescriptors[i].Id, propertyDescriptors[i].Attributes, false, false, false, scriptContext);
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SimpleDictionaryTypeHandlerBase(Recycler * recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked, bool isShared) :
        // Do not RoundUp passed in slotCapacity. This may be called by ConvertTypeHandler for an existing DynamicObject and should use the real existing slotCapacity.
        DynamicTypeHandler(slotCapacity, inlineSlotCapacity, offsetOfInlineSlots, DefaultFlags | (isLocked ? IsLockedFlag : 0) | (isShared ? (MayBecomeSharedFlag | IsSharedFlag) : 0)),
        nextPropertyIndex(0),
        singletonInstance(nullptr),
        isUnordered(false),
        hasNamelessPropertyId(false),
        numDeletedProperties(0)
    {
        SetIsInlineSlotCapacityLocked();
        Assert(slotCapacity <= MaxPropertyIndexSize);
        propertyMap = RecyclerNew(recycler, SimplePropertyDescriptorMap, recycler, this->GetSlotCapacity());
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SimpleDictionaryTypeHandlerBase(Recycler* recycler, int slotCapacity, int propertyCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false) :
        // Do not RoundUp passed in slotCapacity. This may be called by ConvertTypeHandler for an existing DynamicObject and should use the real existing slotCapacity.
        DynamicTypeHandler(slotCapacity, inlineSlotCapacity, offsetOfInlineSlots, DefaultFlags | (isLocked ? IsLockedFlag : 0) | (isShared ? (MayBecomeSharedFlag | IsSharedFlag) : 0)),
        nextPropertyIndex(0),
        singletonInstance(nullptr),
        isUnordered(false),
        hasNamelessPropertyId(false),
        numDeletedProperties(0)
    {
        SetIsInlineSlotCapacityLocked();
        Assert(slotCapacity <= MaxPropertyIndexSize);

        propertyMap = RecyclerNew(recycler, SimplePropertyDescriptorMap, recycler, propertyCapacity);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::DoShareTypeHandler(ScriptContext* scriptContext)
    {
        Assert((GetFlags() & (IsLockedFlag | MayBecomeSharedFlag | IsSharedFlag)) == (IsLockedFlag | MayBecomeSharedFlag));
        Assert(HasSingletonInstanceOnlyIfNeeded());

        // If this handler is becoming shared we need to remove the singleton instance (so that it can be collected
        // if no longer referenced by anything else) and invalidate any fixed fields.

        // The propertyMap dictionary is guaranteed to have contiguous entries because we never remove entries from it.
        for (int index = 0; index < propertyMap->Count(); index++)
        {
            TMapKey propertyKey = propertyMap->GetKeyAt(index);
            SimpleDictionaryPropertyDescriptor<TPropertyIndex>* const descriptor = propertyMap->GetReferenceAt(index);
            descriptor->isInitialized = true;
            InvalidateFixedField(propertyKey, descriptor, scriptContext);
        }

        this->singletonInstance = nullptr;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <bool check__proto__>
    DynamicType* SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::InternalCreateTypeForNewScObject(ScriptContext* scriptContext, DynamicType* type, const Js::PropertyIdArray *propIds, bool shareType)
    {
        Recycler* recycler = scriptContext->GetRecycler();
        uint count = propIds->count;
        Assert(count <= static_cast<uint>(MaxPropertyIndexSize));

        SimpleDictionaryTypeHandlerBase* typeHandler = SimpleDictionaryTypeHandlerBase::New(recycler, count,
            type->GetTypeHandler()->GetInlineSlotCapacity(), type->GetTypeHandler()->GetOffsetOfInlineSlots(), true, shareType);
        if (!shareType) typeHandler->SetMayBecomeShared();

        for (uint i = 0; i < count; i++)
        {
            //
            // For a function with same named parameters,
            // property id Constants::NoProperty will be passed for all the dupes except the last one
            // We need to allocate space for dupes, but don't add those to map
            //
            PropertyId propertyId = propIds->elements[i];
            const PropertyRecord* propertyRecord = propertyId == Constants::NoProperty ? NULL : scriptContext->GetPropertyName(propertyId);
            PropertyAttributes attr = PropertyRecord::DefaultAttributesForPropertyId(propertyId, check__proto__);
            typeHandler->Add(propertyRecord, attr, shareType, false, false, scriptContext);
        }

        Assert((typeHandler->GetFlags() & IsPrototypeFlag) == 0);

#ifdef PROFILE_OBJECT_LITERALS
        scriptContext->objectLiteralSimpleDictionaryCount++;
#endif

        return RecyclerNew(recycler, DynamicType, type, typeHandler, /* isLocked = */ true, /* isShared = */ shareType);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    DynamicType* SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::CreateTypeForNewScObject(ScriptContext* scriptContext, DynamicType* type, const Js::PropertyIdArray *propIds, bool shareType, bool check__proto__)
    {
        if (check__proto__)
        {
            return InternalCreateTypeForNewScObject<true>(scriptContext, type, propIds, shareType);
        }
        else
        {
            return InternalCreateTypeForNewScObject<false>(scriptContext, type, propIds, shareType);
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    int SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetPropertyCount()
    {
        return propertyMap->Count();
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SupportsSwitchingToUnordered(
        const ScriptContext *const scriptContext) const
    {
        Assert(scriptContext);
        return
            !isUnordered &&
            CONFIG_FLAG(DeletedPropertyReuseThreshold) > 0;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    SimpleDictionaryUnorderedTypeHandler<TPropertyIndex, TMapKey, IsNotExtensibleSupported> *SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::AsUnordered()
    {
        return static_cast<SimpleDictionaryUnorderedTypeHandler<TPropertyIndex, TMapKey, IsNotExtensibleSupported> *>(this);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetNumDeletedProperties(const byte n)
    {
        numDeletedProperties = n;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <typename U, typename UMapKey>
    U* SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::ConvertToTypeHandler(DynamicObject* instance)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();

        U* newTypeHandler = RecyclerNew(recycler, U, recycler, GetSlotCapacity(), GetInlineSlotCapacity(), GetOffsetOfInlineSlots());
        // We expect the new type handler to start off marked as having only writable data properties.
        Assert(newTypeHandler->GetHasOnlyWritableDataProperties());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        DynamicType* oldType = instance->GetDynamicType();
        RecyclerWeakReference<DynamicObject>* oldSingletonInstance = GetSingletonInstance();
        TraceFixedFieldsBeforeTypeHandlerChange(L"SimpleDictionaryTypeHandler", L"[Simple]DictionaryTypeHandler", instance, this, oldType, oldSingletonInstance);
#endif

        // We assume the new type handler is not shared.  Hence it's ok to set this instance as the handler's singleton instance.
        Assert(HasSingletonInstanceOnlyIfNeeded());
        if (AreSingletonInstancesNeeded())
        {
            RecyclerWeakReference<DynamicObject>* curSingletonInstance = this->singletonInstance;
            if (curSingletonInstance != nullptr && curSingletonInstance->Get() == instance)
            {
                newTypeHandler->SetSingletonInstance(curSingletonInstance);
            }
            else
            {
                newTypeHandler->SetSingletonInstance(instance->CreateWeakReferenceToSelf());
            }
        }

        // If we are a prototype or may become a prototype we must transfer used as fixed bits.
        // See point 4 in PathTypeHandlerBase::ConvertToSimpleDictionaryType.
        bool isGlobalObject = instance->GetTypeId() == TypeIds_GlobalObject;
        bool isTypeLocked = instance->GetDynamicType()->GetIsLocked();
        bool isOrMayBecomeShared = GetIsOrMayBecomeShared();
        Assert(!isOrMayBecomeShared || !IsolatePrototypes() || ((this->GetFlags() & IsPrototypeFlag) == 0));
        // For the global object we don't emit a type check before a hard-coded use of a fixed field. Therefore a type transition isn't sufficient to
        // invalidate any used fixed fields, and we must continue tracking them on the new type handler. If the type isn't locked, we may not change the
        // type of the instance, and we must also track the used fixed fields on the new handler.
        bool transferUsedAsFixed = isGlobalObject || !isTypeLocked || ((this->GetFlags() & IsPrototypeFlag) != 0 || (isOrMayBecomeShared && !IsolatePrototypes())) || PHASE_FORCE1(Js::FixDataPropsPhase);

        SimpleDictionaryPropertyDescriptor<TPropertyIndex> descriptor;
        TMapKey propertyKey;
        for (int i = 0; i < propertyMap->Count(); i++)
        {
            descriptor = propertyMap->GetValueAt(i);
            propertyKey = propertyMap->GetKeyAt(i);

            // newTH->nextPropertyIndex will be less than desc.propertyIndex, when we have function with same name parameters
            if (newTypeHandler->nextPropertyIndex < (U::PropertyIndexType)descriptor.propertyIndex)
            {
                newTypeHandler->nextPropertyIndex = (U::PropertyIndexType)descriptor.propertyIndex;
            }

            Assert(newTypeHandler->nextPropertyIndex == descriptor.propertyIndex);
            Assert(!GetIsShared() || !descriptor.isFixed);
            newTypeHandler->Add(TMapKey_ConvertKey<UMapKey>(scriptContext, propertyKey), descriptor.Attributes, descriptor.isInitialized, descriptor.isFixed, transferUsedAsFixed && descriptor.usedAsFixed, scriptContext);
        }

        newTypeHandler->nextPropertyIndex = (U::PropertyIndexType)nextPropertyIndex;
        newTypeHandler->SetNumDeletedProperties(numDeletedProperties);

        ClearSingletonInstance();

        AssertMsg((newTypeHandler->GetFlags() & IsPrototypeFlag) == 0, "Why did we create a brand new type handler with a prototype flag set?");
        newTypeHandler->SetFlags(IsPrototypeFlag, this->GetFlags());
        newTypeHandler->ChangeFlags(IsExtensibleFlag | IsSealedOnceFlag | IsFrozenOnceFlag, this->GetFlags());
        // Any new type handler we expect to see here should have inline slot capacity locked.  If this were to change, we would need
        // to update our shrinking logic (see PathTypeHandlerBase::ShrinkSlotAndInlineSlotCapacity).
        Assert(newTypeHandler->GetIsInlineSlotCapacityLocked());
        newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection, this->GetPropertyTypes());
        newTypeHandler->SetInstanceTypeHandler(instance);
        // We assumed that we don't need to transfer used as fixed bits unless we are a prototype, which is only valid if we also changed the type.
        Assert(transferUsedAsFixed || (instance->GetType() != oldType && oldType->GetTypeId() != TypeIds_GlobalObject));
        Assert(!newTypeHandler->HasSingletonInstance() || !instance->HasSharedType());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        TraceFixedFieldsAfterTypeHandlerChange(instance, this, newTypeHandler, oldType, oldSingletonInstance);
#endif

        return newTypeHandler;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    DictionaryTypeHandlerBase<TPropertyIndex>* SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::ConvertToDictionaryType(DynamicObject* instance)
    {
        DictionaryTypeHandlerBase<TPropertyIndex>* newTypeHandler = ConvertToTypeHandler<DictionaryTypeHandlerBase<TPropertyIndex>, const PropertyRecord*>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertSimpleDictionaryToDictionaryCount++;
#endif
        return newTypeHandler;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    ES5ArrayTypeHandlerBase<TPropertyIndex>* SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::ConvertToES5ArrayType(DynamicObject* instance)
    {
        ES5ArrayTypeHandlerBase<TPropertyIndex>* newTypeHandler = ConvertToTypeHandler<ES5ArrayTypeHandlerBase<TPropertyIndex>, const PropertyRecord*>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertSimpleDictionaryToDictionaryCount++;
#endif
        return newTypeHandler;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>* SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::ConvertToNonSharedSimpleDictionaryType(DynamicObject* instance)
    {
        // Although an unordered type handler is never actually shared, it can be flagged as shared by type snapshot enumeration
        // to freeze the initial type handler before enumeration commences
        SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>* newTypeHandler =
            isUnordered
                ? ConvertToSimpleDictionaryUnorderedTypeHandler<TPropertyIndex, TMapKey, IsNotExtensibleSupported>(instance)
                : ConvertToTypeHandler<SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>, TMapKey>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertSimpleSharedDictionaryToNonSharedCount++;
#endif
        return newTypeHandler;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <typename NewTPropertyIndex, typename NewTMapKey, bool NewIsNotExtensibleSupported>
    SimpleDictionaryUnorderedTypeHandler<NewTPropertyIndex, NewTMapKey, NewIsNotExtensibleSupported>* SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::ConvertToSimpleDictionaryUnorderedTypeHandler(DynamicObject* instance)
    {
        CompileAssert(sizeof(NewTPropertyIndex) >= sizeof(TPropertyIndex));
        Assert(instance);

        SimpleDictionaryUnorderedTypeHandler<NewTPropertyIndex, NewTMapKey, NewIsNotExtensibleSupported> *const newTypeHandler =
            ConvertToTypeHandler<SimpleDictionaryUnorderedTypeHandler<NewTPropertyIndex, NewTMapKey, NewIsNotExtensibleSupported>, NewTMapKey>(instance);

        if(isUnordered)
        {
            newTypeHandler->CopyUnorderedStateFrom(*AsUnordered());
        }
        else
        {
            for(int i = 0; i < propertyMap->Count(); ++i)
            {
                SimpleDictionaryPropertyDescriptor<TPropertyIndex> descriptor(propertyMap->GetValueAt(i));
                if(descriptor.Attributes & PropertyDeleted)
                {
                    newTypeHandler->TryRegisterDeletedPropertyIndex(instance, descriptor.propertyIndex);
                }
            }
        }

        return newTypeHandler;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    PropertyId SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetPropertyId(ScriptContext* scriptContext, PropertyIndex index)
    {
        if (index < propertyMap->Count() && !(propertyMap->GetValueAt(index).Attributes & (PropertyDeleted | PropertyLetConstGlobal)))
        {
            return TMapKey_GetPropertyId(scriptContext, propertyMap->GetKeyAt(index));
        }
        else
        {
            return Constants::NoProperty;
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    PropertyId SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index)
    {
        if (index < propertyMap->Count() && !(propertyMap->GetValueAt(index).Attributes & (PropertyDeleted | PropertyLetConstGlobal)))
        {
            return TMapKey_GetPropertyId(scriptContext, propertyMap->GetKeyAt(index));
        }
        else
        {
            return Constants::NoProperty;
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyStringName,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        Assert(propertyStringName);
        Assert(propertyId);
        Assert(type);
        Assert(typeToEnumerate);

        if(type == typeToEnumerate)
        {
            for( ; index < propertyMap->Count(); ++index )
            {
                SimpleDictionaryPropertyDescriptor<TPropertyIndex> descriptor(propertyMap->GetValueAt(index));
                if( !(descriptor.Attributes & (PropertyDeleted | PropertyLetConstGlobal)) && (!requireEnumerable || (descriptor.Attributes & PropertyEnumerable)))
                {
                    TMapKey key = propertyMap->GetKeyAt(index);

                    // Skip this property if it is a symbol and we are not including symbol properties
                    if (!enumSymbols && TMapKey_IsSymbol(key, scriptContext))
                    {
                        continue;
                    }

                    if (attributes != nullptr)
                    {
                        *attributes = descriptor.Attributes;
                    }

                    *propertyId = TMapKey_GetPropertyId(scriptContext, key);
                    PropertyString* propertyString = type->GetScriptContext()->GetPropertyString(*propertyId);
                    *propertyStringName = propertyString;
                    if (descriptor.Attributes & PropertyWritable)
                    {
                        uint16 inlineOrAuxSlotIndex;
                        bool isInlineSlot;
                        PropertyIndexToInlineOrAuxSlotIndex(descriptor.propertyIndex, &inlineOrAuxSlotIndex, &isInlineSlot);

                        propertyString->UpdateCache(type, inlineOrAuxSlotIndex, isInlineSlot, descriptor.isInitialized && !descriptor.isFixed);
                    }
                    else
                    {
#ifdef DEBUG
                        PropertyCache const* cache = propertyString->GetPropertyCache();
                        Assert(!cache || cache->type != type);
#endif
                    }

                    return TRUE;
                }
            }

            return FALSE;
        }

        // Need to enumerate a different type than the current one. This is because type snapshot enumerate is enabled and the
        // object's type changed since enumeration began, so need to enumerate properties of the initial type.
        DynamicTypeHandler *const typeHandlerToEnumerate = typeToEnumerate->GetTypeHandler();
        for(
            ;
            typeHandlerToEnumerate->FindNextProperty(
                scriptContext,
                index,
                propertyStringName,
                propertyId,
                attributes,
                typeToEnumerate,
                typeToEnumerate,
                requireEnumerable,
                enumSymbols);
            ++index)
        {
            SimpleDictionaryPropertyDescriptor<TPropertyIndex> descriptor;
            bool hasValue = false;
            if (*propertyId != Constants::NoProperty)
            {
                PropertyRecord const* propertyRecord = type->GetScriptContext()->GetPropertyName(*propertyId);

                AssertMsg(enumSymbols || !propertyRecord->IsSymbol(),
                    "typeHandlerToEnumerate->FindNextProperty call above should not have returned us a symbol if we are not enumerating symbols");

                hasValue = propertyMap->TryGetValue(propertyRecord, &descriptor);
            }
            else if (*propertyStringName != nullptr)
            {
                hasValue = propertyMap->TryGetValue(*propertyStringName, &descriptor);
            }

            if (hasValue &&
                !(descriptor.Attributes & (PropertyDeleted | PropertyLetConstGlobal)) &&
                (!requireEnumerable || descriptor.Attributes & PropertyEnumerable))
            {
                if (attributes != nullptr)
                {
                    *attributes = descriptor.Attributes;
                }

                if(descriptor.Attributes & PropertyWritable)
                {
                    uint16 inlineOrAuxSlotIndex;
                    bool isInlineSlot;
                    PropertyIndexToInlineOrAuxSlotIndex(descriptor.propertyIndex, &inlineOrAuxSlotIndex, &isInlineSlot);
                    if (VirtualTableInfo<PropertyString>::HasVirtualTable(*propertyStringName))
                    {
                        PropertyString* propertyString = (PropertyString*)(*propertyStringName);
                        propertyString->UpdateCache(type, inlineOrAuxSlotIndex, isInlineSlot, descriptor.isInitialized && !descriptor.isFixed);
                    }
                }
                else
                {
#ifdef DEBUG
                    if (VirtualTableInfo<PropertyString>::HasVirtualTable(*propertyStringName))
                    {
                        PropertyString* propertyString = (PropertyString*)(*propertyStringName);
                        PropertyCache const* cache = propertyString->GetPropertyCache();
                        Assert(!cache || cache->type != type);
                    }
#endif
                }

                return TRUE;
            }
        }

        return FALSE;
    }

    // Note on template specializations:
    // C++ doesn't allow us to specify partially specialized template member function and requires all parameters,
    // like this: template<bool B> PropertyIndex SimpleDictionaryTypeHandlerBase<PropertyIndex, B>::GetPropertyIndex().
    // Since we don't care about the boolean in this template method, just delegate to the other function.


#define DefineUnusedSpecialization_FindNextProperty_BigPropertyIndex(T, S) \
    template <> BOOL SimpleDictionaryTypeHandlerBase<BigPropertyIndex, T, S>::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyString, PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols) { Throw::InternalError(); }

    DefineUnusedSpecialization_FindNextProperty_BigPropertyIndex(const PropertyRecord*, false)
    DefineUnusedSpecialization_FindNextProperty_BigPropertyIndex(const PropertyRecord*, true)
    DefineUnusedSpecialization_FindNextProperty_BigPropertyIndex(JavascriptString*, false)
    DefineUnusedSpecialization_FindNextProperty_BigPropertyIndex(JavascriptString*, true)

#undef DefineUnusedSpecialization_FindNextProperty_BigPropertyIndex

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyString,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        PropertyIndex local = (PropertyIndex)index;
        Assert(index <= Constants::UShortMaxValue || index == Constants::NoBigSlot);
        BOOL result = this->FindNextProperty(scriptContext, local, propertyString, propertyId, attributes, type, typeToEnumerate, requireEnumerable, enumSymbols);
        index = local;
        return result;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    __inline BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::FindNextProperty_BigPropertyIndex(ScriptContext* scriptContext, TPropertyIndex& index,
        JavascriptString** propertyStringName, PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        Assert(propertyStringName);
        Assert(propertyId);
        Assert(type);
        Assert(typeToEnumerate);

        if(type == typeToEnumerate)
        {
            for( ; index < propertyMap->Count(); ++index )
            {
                SimpleDictionaryPropertyDescriptor<TPropertyIndex> descriptor(propertyMap->GetValueAt(index));
                if( !(descriptor.Attributes & (PropertyDeleted | PropertyLetConstGlobal)) && (!requireEnumerable || (descriptor.Attributes & PropertyEnumerable)))
                {
                    auto key = propertyMap->GetKeyAt(index);

                    // Skip this property if it is a symbol and we are not including symbol properties
                    if (!enumSymbols && TMapKey_IsSymbol(key, scriptContext))
                    {
                        continue;
                    }

                    if (attributes != nullptr)
                    {
                        *attributes = descriptor.Attributes;
                    }

                    *propertyId = TMapKey_GetPropertyId(scriptContext, key);
                    *propertyStringName = type->GetScriptContext()->GetPropertyString(*propertyId);

                    return TRUE;
                }
            }

            return FALSE;
        }

        // Need to enumerate a different type than the current one. This is because type snapshot enumerate is enabled and the
        // object's type changed since enumeration began, so need to enumerate properties of the initial type.
        DynamicTypeHandler *const typeHandlerToEnumerate = typeToEnumerate->GetTypeHandler();
        for(
            ;
            typeHandlerToEnumerate->FindNextProperty(
                scriptContext,
                index,
                propertyStringName,
                propertyId,
                attributes,
                typeToEnumerate,
                typeToEnumerate,
                requireEnumerable,
                enumSymbols);
            ++index)
        {
            SimpleDictionaryPropertyDescriptor<TPropertyIndex> descriptor;
            bool hasValue = false;
            if (*propertyId != Constants::NoProperty)
            {
                PropertyRecord const* propertyRecord = type->GetScriptContext()->GetPropertyName(*propertyId);

                AssertMsg(enumSymbols || !propertyRecord->IsSymbol(),
                    "typeHandlerToEnumerate->FindNextProperty call above should not have returned us a symbol if we are not enumerating symbols");

                hasValue = propertyMap->TryGetValue(propertyRecord, &descriptor);
            }
            else if (*propertyStringName != nullptr)
            {
                hasValue = propertyMap->TryGetValue(*propertyStringName, &descriptor);
            }
            if (hasValue &&
                !(descriptor.Attributes & (PropertyDeleted | PropertyLetConstGlobal)) &&
                (!requireEnumerable || descriptor.Attributes & PropertyEnumerable))
            {
                if (attributes != nullptr)
                {
                    *attributes = descriptor.Attributes;
                }

#ifdef DEBUG
                if (VirtualTableInfo<PropertyString>::HasVirtualTable(*propertyStringName))
                {
                    PropertyCache const* cache = ((PropertyString*)(*propertyStringName))->GetPropertyCache();
                    Assert(!cache || cache->type != type);
                }
#endif

                return TRUE;
            }
        }

        return FALSE;
    }

    template <>
    BOOL SimpleDictionaryTypeHandlerBase<BigPropertyIndex, const PropertyRecord*, false>::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyString,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        return this->FindNextProperty_BigPropertyIndex(scriptContext, index, propertyString, propertyId, attributes, type, typeToEnumerate, requireEnumerable, enumSymbols);
    }

    template <>
    BOOL SimpleDictionaryTypeHandlerBase<BigPropertyIndex, const PropertyRecord*, true>::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyString,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        return this->FindNextProperty_BigPropertyIndex(scriptContext, index, propertyString, propertyId, attributes, type, typeToEnumerate, requireEnumerable, enumSymbols);
    }

    template <>
    BOOL SimpleDictionaryTypeHandlerBase<BigPropertyIndex, JavascriptString*, false>::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyString,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        return this->FindNextProperty_BigPropertyIndex(scriptContext, index, propertyString, propertyId, attributes, type, typeToEnumerate, requireEnumerable, enumSymbols);
    }

    template <>
    BOOL SimpleDictionaryTypeHandlerBase<BigPropertyIndex, JavascriptString*, true>::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyString,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        return this->FindNextProperty_BigPropertyIndex(scriptContext, index, propertyString, propertyId, attributes, type, typeToEnumerate, requireEnumerable, enumSymbols);
    }

    template <typename TPropertyIndex>
    __inline PropertyIndex DisallowBigPropertyIndex(TPropertyIndex index)
    {
        if (index <= Constants::PropertyIndexMax)
        {
            return (PropertyIndex)index;
        }
        return Constants::NoSlot;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <bool allowLetConstGlobal>
    __inline PropertyIndex SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetPropertyIndex_Internal(const PropertyRecord* propertyRecord)
    {
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        if (propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & (PropertyDeleted | (!allowLetConstGlobal ? PropertyLetConstGlobal : 0))))
        {
            return DisallowBigPropertyIndex(descriptor->propertyIndex);
        }
        return Constants::NoSlot;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    PropertyIndex SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetPropertyIndex(const PropertyRecord* propertyRecord)
    {
        return this->GetPropertyIndex_Internal<false>(propertyRecord);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    PropertyIndex SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetRootPropertyIndex(const PropertyRecord* propertyRecord)
    {
        return this->GetPropertyIndex_Internal<true>(propertyRecord);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info)
    {
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        if (propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {
            Js::PropertyIndex absSlotIndex = DisallowBigPropertyIndex(descriptor->propertyIndex);
            info.slotIndex = AdjustSlotIndexForInlineSlots(absSlotIndex);
            info.isAuxSlot = absSlotIndex > GetInlineSlotCapacity();
            info.isWritable = !!(descriptor->Attributes & PropertyWritable);
        }
        else
        {
            info.slotIndex = Constants::NoSlot;
            info.isAuxSlot = false;
            info.isWritable = false;
        }
        return info.slotIndex != Constants::NoSlot;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex)
    {
        uint propertyCount = record.propertyCount;
        Js::EquivalentPropertyEntry* properties = record.properties;
        for (uint pi = 0; pi < propertyCount; pi++)
        {
            const EquivalentPropertyEntry* refInfo = &properties[pi];
            if (!this->IsObjTypeSpecEquivalentImpl<false>(type, refInfo))
            {
                failedPropertyIndex = pi;
                return false;
            }
        }

        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry *entry)
    {
        return this->IsObjTypeSpecEquivalentImpl<true>(type, entry);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <bool doLock>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::IsObjTypeSpecEquivalentImpl(const Type* type, const EquivalentPropertyEntry *entry)
    {
        TPropertyIndex absSlotIndex = Constants::NoSlot;
        PropertyIndex relSlotIndex = Constants::NoSlot;

        ScriptContext* scriptContext = type->GetScriptContext();

        const PropertyRecord* propertyRecord =
            doLock ? scriptContext->GetPropertyNameLocked(entry->propertyId) : scriptContext->GetPropertyName(entry->propertyId);
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        if (this->propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {
            absSlotIndex = descriptor->propertyIndex;
            if (absSlotIndex <= Constants::PropertyIndexMax)
            {
                relSlotIndex = AdjustValidSlotIndexForInlineSlots(static_cast<PropertyIndex>(absSlotIndex));
            }
        }

        if (relSlotIndex != Constants::NoSlot)
        {
            if (relSlotIndex != entry->slotIndex || ((absSlotIndex >= GetInlineSlotCapacity()) != entry->isAuxSlot))
            {
                return false;
            }

            if (entry->mustBeWritable && (!(descriptor->Attributes & PropertyWritable) || !descriptor->isInitialized || descriptor->isFixed))
            {
                return false;
            }
        }
        else
        {
            if (entry->slotIndex != Constants::NoSlot || entry->mustBeWritable)
            {
                return false;
            }
        }

        return true;
    }

    // The following template specialization is required in order to provide an implementation of
    // Add for the linker to find that TypePathHandler uses. The following definition should have sufficed.
    template<>
    template<>
    void SimpleDictionaryTypeHandlerBase<PropertyIndex, const PropertyRecord*, false>::Add(
        const PropertyRecord* propertyRecord,
        PropertyAttributes attributes,
        ScriptContext* const scriptContext)
    {
        Add(propertyRecord, attributes, true, false, false, scriptContext);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <typename TPropertyKey>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::Add(
        TPropertyKey propertyKey,
        PropertyAttributes attributes,
        ScriptContext *const scriptContext)
    {
        Add(propertyKey, attributes, true, false, false, scriptContext);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <typename TPropertyKey>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::Add(
        TPropertyKey propertyKey,
        PropertyAttributes attributes,
        bool isInitialized, bool isFixed, bool usedAsFixed,
        ScriptContext *const scriptContext)
    {
        Assert(this->GetSlotCapacity() <= MaxPropertyIndexSize);   // slotCapacity should never exceed MaxPropertyIndexSize
        Assert(nextPropertyIndex < this->GetSlotCapacity());       // nextPropertyIndex must be ready

        Add(nextPropertyIndex++, propertyKey, attributes, isInitialized, isFixed, usedAsFixed, scriptContext);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <typename TPropertyKey>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::Add(
        TPropertyIndex propertyIndex,
        TPropertyKey propertyKey,
        PropertyAttributes attributes,
        ScriptContext *const scriptContext)
    {
        Add(propertyIndex, propertyKey, attributes, true, false, false, scriptContext);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <typename TPropertyKey>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::Add(
        TPropertyIndex propertyIndex,
        TPropertyKey propertyKey,
        PropertyAttributes attributes,
        bool isInitialized, bool isFixed, bool usedAsFixed,
        ScriptContext *const scriptContext)
    {
        //
        // For a function with same named parameters,
        // property id Constants::NoProperty will be passed for all the dups except the last one
        // We need to allocate space for dups, but don't add those to map
        if (propertyKey != NULL)
        {
            SimpleDictionaryPropertyDescriptor<TPropertyIndex> descriptor(propertyIndex, attributes);
            Assert((!isFixed && !usedAsFixed) || (!TPropertyKey_IsInternalPropertyId(propertyKey) && this->singletonInstance != nullptr));
            if (TPropertyKey_IsInternalPropertyId(propertyKey) || TMapKey_IsSymbol(propertyKey, scriptContext))
            {
                Assert(!TMapKey_IsJavascriptString<TMapKey>());
                hasNamelessPropertyId = true;
            }
            descriptor.isInitialized = isInitialized;
            descriptor.isFixed = isFixed;
            descriptor.usedAsFixed = usedAsFixed;
            propertyMap->Add(TMapKey_ConvertKey<TMapKey>(scriptContext, propertyKey), descriptor);
        }

        if (!(attributes & PropertyWritable))
        {
            this->ClearHasOnlyWritableDataProperties();
            if (GetFlags() & IsPrototypeFlag)
            {
                scriptContext->InvalidateStoreFieldCaches(TMapKey_GetPropertyId(scriptContext, propertyKey));
                scriptContext->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::HasProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl)
    {
        return HasProperty_Internal<false>(instance, propertyId, noRedecl, nullptr);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::HasRootProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl, bool *pDeclaredProperty)
    {
        return HasProperty_Internal<true>(instance, propertyId, noRedecl, pDeclaredProperty);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <bool allowLetConstGlobal>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::HasProperty_Internal(DynamicObject* instance, PropertyId propertyId, bool *noRedecl, bool *pDeclaredProperty)
    {
        // HasProperty is called with NoProperty in JavascriptDispatch.cpp to for undeferral of the
        // deferred type system that DOM objects use.  Allow NoProperty for this reason, but only
        // here in HasProperty.
        if (propertyId == Constants::NoProperty)
        {
            return false;
        }

        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if ((descriptor->Attributes & PropertyDeleted) || (!allowLetConstGlobal && !descriptor->HasNonLetConstGlobal()))
            {
                return false;
            }
            if (noRedecl && descriptor->Attributes & PropertyNoRedecl)
            {
                *noRedecl = true;
            }
            if (pDeclaredProperty && descriptor->Attributes & (PropertyNoRedecl | PropertyDeclaredGlobal))
            {
                *pDeclaredProperty = true;
            }
            return true;
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {
            return SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::HasItem(instance, propertyRecord->GetNumericValue());
        }

        return false;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {
            if (descriptor->Attributes & (PropertyDeleted | PropertyLetConstGlobal))
            {
                return false;
            }
            return true;
        }

        return false;
    }


    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetRootProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return GetProperty_Internal<true>(instance, originalInstance, propertyId, value, info, requestContext);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return GetProperty_Internal<false>(instance, originalInstance, propertyId, value, info, requestContext);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <bool allowLetConstGlobal>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetProperty_Internal(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            return GetPropertyFromDescriptor<allowLetConstGlobal>(instance, descriptor, value, info);
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {
            return SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetItem(instance, originalInstance, propertyRecord->GetNumericValue(), value, requestContext);
        }

        return false;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {
            return GetPropertyFromDescriptor<false>(instance, descriptor, value, info);
        }

        return false;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <bool allowLetConstGlobal>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetPropertyFromDescriptor(DynamicObject* instance, SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor, Var* value, PropertyValueInfo* info)
    {
        if (descriptor->Attributes & (PropertyDeleted | (allowLetConstGlobal ? 0 : PropertyLetConstGlobal)))
        {
            return false;
        }
        if (descriptor->propertyIndex != NoSlots)
        {
            *value = instance->GetSlot(descriptor->propertyIndex);
            SetPropertyValueInfo(info, instance, descriptor->propertyIndex, descriptor->Attributes);
            if (!descriptor->isInitialized || descriptor->isFixed)
            {
                PropertyValueInfo::DisableStoreFieldCache(info);
            }
        }
        else
        {
            *value = instance->GetLibrary()->GetUndefined();
        }
        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        return SetProperty_JavascriptString(instance, propertyNameString, value, flags, info, TemplateParameter::Box<TMapKey>());
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetProperty_JavascriptString(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info, TemplateParameter::Box<const PropertyRecord*>)
    {
        // Either the property exists in the dictionary, in which case a PropertyRecord for it exists,
        // or we have to add it to the dictionary, in which case we need to get or create a PropertyRecord.
        // Thus, just get or create one and call the PropertyId overload of SetProperty.
        PropertyRecord const * propertyRecord;
        instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetProperty(instance, propertyRecord->GetPropertyId(), value, flags, info);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetProperty_JavascriptString(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info, TemplateParameter::Box<JavascriptString*>)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {
            return SetPropertyFromDescriptor<false>(instance, Constants::NoProperty, propertyNameString, descriptor, value, flags, info);
        }

        return this->AddProperty(instance, propertyNameString, value, PropertyDynamicTypeDefaults, info, flags, SideEffects_Any);
    }

#define DefineUnusedSpecialization_SetProperty_JavascriptString(T,S) \
    template<> BOOL SimpleDictionaryTypeHandlerBase<T, const PropertyRecord*, S>::SetProperty_JavascriptString(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info, TemplateParameter::Box<JavascriptString*>) { Throw::InternalError(); }

    DefineUnusedSpecialization_SetProperty_JavascriptString(PropertyIndex, true)
    DefineUnusedSpecialization_SetProperty_JavascriptString(PropertyIndex, false)
    DefineUnusedSpecialization_SetProperty_JavascriptString(BigPropertyIndex, true)
    DefineUnusedSpecialization_SetProperty_JavascriptString(BigPropertyIndex, false)

#undef DefineUnusedSpecialization_SetProperty_JavascriptString

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <bool allowLetConstGlobal>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetProperty_Internal(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);

        // It can be the case that propertyRecord is a symbol property and it has the same string description as
        // another property which is in the propertyMap. If we are in a string-keyed type handler, the propertyMap
        // will find that normal property when we call TryGetReference with the symbol propertyRecord. However,
        // we don't want to update that descriptor with value since the two properties are actually different.
        // In fact, we can't store a symbol in a string-keyed type handler at all since the string description
        // is not used for symbols. Instead, we want to skip searching for the descriptor if we are in a string-keyed
        // type handler. When we call AddProperty with the symbol propertyRecord, it should convert us to a
        // const PropertyRecord* - keyed type handler anyway.
        if (!(TMapKey_IsJavascriptString<TMapKey>() && propertyRecord->IsSymbol())
            && propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            return SetPropertyFromDescriptor<allowLetConstGlobal>(instance, propertyId, propertyId, descriptor, value, flags, info);
        }

        // Always check numeric propertyId. This may create objectArray.
        if (propertyRecord->IsNumeric())
        {
            return this->SetItem(instance, propertyRecord->GetNumericValue(), value, flags);
        }

        return this->AddProperty(instance, propertyRecord, value, PropertyDynamicTypeDefaults, info, flags, SideEffects_Any);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        return SetProperty_Internal<false>(instance, propertyId, value, flags, info);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetRootProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return SetProperty_Internal<true>(instance, propertyId, value, flags, info);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <bool allowLetConstGlobal, typename TPropertyKey>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetPropertyFromDescriptor(DynamicObject* instance, PropertyId propertyId, TPropertyKey propertyKey, SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        Assert(instance);
        ScriptContext* scriptContext = instance->GetScriptContext();
        bool throwIfNotExtensible = (flags & (PropertyOperation_ThrowIfNotExtensible | PropertyOperation_StrictMode)) != 0;

        if (!allowLetConstGlobal && (descriptor->Attributes & PropertyLetConstGlobal))
        {
            // We have a shadowing case here.  An undeclared global property
            // is being added after a let/const was already declared.
            //
            // SimpleDictionaryTypeHandlerBase does not handle shadowed globals
            // because we do not want to add another property index field to the
            // property descriptors.  Instead convert to DictionaryTypeHandler
            // where it will reuse one of the getter/setter fields on its
            // property descriptor type.
            return
                ConvertToDictionaryType(instance)
                    ->SetProperty(
                            instance,
                            propertyId,
                            value,
                            flags,
                            info);
        }

        if (descriptor->Attributes & PropertyDeleted)
        {
            if(GetIsLocked())
            {
                return
                    ConvertToNonSharedSimpleDictionaryType(instance)->SetProperty(instance, propertyKey, value, flags, info);
            }

            if(isUnordered)
            {
                TPropertyIndex propertyIndex;
                if(AsUnordered()->TryUndeleteProperty(instance, descriptor->propertyIndex, &propertyIndex))
                {
                    Assert(PropertyRecordStringHashComparer<TMapKey>::Equals(propertyMap->GetKeyAt(propertyIndex), TMapKey_OptionalConvertPropertyIdToPropertyRecord(scriptContext, propertyKey)));
                    descriptor = propertyMap->GetReferenceAt(propertyIndex);
                }
            }

            if (IsNotExtensibleSupported)
            {
                bool isForce = (flags & PropertyOperation_Force) != 0;
                if (!isForce)
                {
                    if (!this->VerifyIsExtensible(scriptContext, throwIfNotExtensible))
                    {
                        return FALSE;
                    }
                }
            }

            if(SupportsSwitchingToUnordered(scriptContext))
            {
                --numDeletedProperties;
            }
            descriptor->Attributes = PropertyDynamicTypeDefaults;
            instance->SetHasNoEnumerableProperties(false);
            propertyId = TPropertyKey_GetOptionalPropertyId(instance->GetScriptContext(), propertyKey);
            if (propertyId != Constants::NoProperty)
            {
                scriptContext->InvalidateProtoCaches(propertyId);
            }
            descriptor->Attributes = PropertyDynamicTypeDefaults;
        }
        else if (!(descriptor->Attributes & PropertyWritable) && !(flags & PropertyOperation_AllowUndeclInConsoleScope))
        {
            JavascriptError::ThrowCantAssignIfStrictMode(flags, scriptContext);

            // Since we separate LdFld and StFld caches there is no point in caching for StFld with non-writable properties, except perhaps
            // to prepopulate the type property cache (which we do share between LdFld and StFld), for potential future field loads. This
            // would require additional handling in CacheOperators::CachePropertyWrite, such that for !info-IsWritable() we don't populate
            // the local cache (that would be illegal), but still populate the type's property cache.
            PropertyValueInfo::SetNoCache(info, instance);
            return false;
        }

        if (descriptor->propertyIndex != NoSlots)
        {
            if ((descriptor->Attributes & PropertyNoRedecl) && !(flags & PropertyOperation_AllowUndecl))
            {
                Assert(scriptContext->GetConfig()->IsLetAndConstEnabled());
                if (scriptContext->IsUndeclBlockVar(instance->GetSlot(descriptor->propertyIndex)) && !(flags & PropertyOperation_AllowUndeclInConsoleScope))
                {
                    JavascriptError::ThrowReferenceError(scriptContext, JSERR_UseBeforeDeclaration);
                }
            }

            DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
            Assert(this->singletonInstance == nullptr || localSingletonInstance == instance);
            if (!descriptor->isInitialized)
            {
                if ((flags & PropertyOperation_PreInit) == 0)
                {
                    // Consider: It would be nice to assert the slot is actually null.  However, we sometimes pre-initialize to undefined or even
                    // some other special illegal value (for let or const), currently == null.
                    descriptor->isInitialized = true;
                    if (localSingletonInstance == instance &&
                        !TPropertyKey_IsInternalPropertyId(propertyKey) &&
                        (flags & (PropertyOperation_NonFixedValue | PropertyOperation_SpecialValue)) == 0)
                    {
                        Assert(!GetIsShared());
                        Assert(value != nullptr);
                        // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
                        Assert(!instance->IsExternal());
                        descriptor->isFixed = (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : (ShouldFixDataProperties() && CheckHeuristicsForFixedDataProps(instance, propertyId, value)));
                    }
                }
            }
            else
            {
                InvalidateFixedField(TMapKey_OptionalConvertPropertyIdToPropertyRecord(scriptContext, propertyKey), descriptor, instance->GetScriptContext());
            }

            SetSlotUnchecked(instance, descriptor->propertyIndex, value);

            if (descriptor->isInitialized && !descriptor->isFixed)
            {
                SetPropertyValueInfo(info, instance, descriptor->propertyIndex, descriptor->Attributes);
            }
            else
            {
                PropertyValueInfo::SetNoCache(info, instance);
            }
        }

        propertyId = TPropertyKey_GetUpdateSideEffectPropertyId(propertyId, propertyKey);
        if (propertyId != Constants::NoProperty)
        {
            SetPropertyUpdateSideEffect(instance, propertyId, value, SideEffects_Any);
        }
        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    DescriptorFlags SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return GetSetter_Internal<false>(instance, propertyId, setterValue, info, requestContext);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    DescriptorFlags SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetRootSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return GetSetter_Internal<true>(instance, propertyId, setterValue, info, requestContext);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <bool allowLetConstGlobal>
    DescriptorFlags SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetSetter_Internal(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            return GetSetterFromDescriptor<allowLetConstGlobal>(descriptor);
        }

        if (propertyRecord->IsNumeric())
        {
            return SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetItemSetter(instance, propertyRecord->GetNumericValue(), setterValue, requestContext);
        }

        return None;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    DescriptorFlags SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {
            return GetSetterFromDescriptor<false>(descriptor);
        }

        return None;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <bool allowLetConstGlobal>
    DescriptorFlags SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetSetterFromDescriptor(SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor)
    {
        if (descriptor->Attributes & (PropertyDeleted | (!allowLetConstGlobal ? PropertyLetConstGlobal : 0)))
        {
            return None;
        }

        if (descriptor->Attributes & PropertyLetConstGlobal)
        {
            if (descriptor->Attributes & PropertyConst)
            {
                return (DescriptorFlags)(Const|Data);
            }
            Assert(descriptor->Attributes & PropertyLet);
            return WritableData;
        }

        if (descriptor->Attributes & PropertyWritable)
        {
            return WritableData;
        }

        if (descriptor->Attributes & PropertyConst)
        {
            return (DescriptorFlags)(Const|Data);
        }

        return Data;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {
        return DeleteProperty_Internal<false>(instance, propertyId, propertyOperationFlags);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::DeleteRootProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return DeleteProperty_Internal<true>(instance, propertyId, propertyOperationFlags);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <bool allowLetConstGlobal>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::DeleteProperty_Internal(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {
        if(!GetIsLocked())
        {
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            if (CONFIG_FLAG(ForceStringKeyedSimpleDictionaryTypeHandler) &&
                !TMapKey_IsJavascriptString<TMapKey>() &&
                !isUnordered && !hasNamelessPropertyId)
            {
                return ConvertToSimpleDictionaryUnorderedTypeHandler<TPropertyIndex, JavascriptString*, IsNotExtensibleSupported>(instance)
                    ->DeleteProperty(instance, propertyId, propertyOperationFlags);
            }
#endif

            ScriptContext* scriptContext = instance->GetScriptContext();
            SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
            Assert(propertyId != Constants::NoProperty);
            PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
            if (propertyMap->TryGetReference(propertyRecord, &descriptor))
            {
                if (descriptor->Attributes & (PropertyDeleted | (!allowLetConstGlobal ? PropertyLetConstGlobal : 0)))
                {
                    // If PropertyLetConstGlobal is present then we have a let/const and no global property,
                    // since SimpleDictionaryTypeHandler does not support shadowing which means it can only
                    // have one or the other.  Therefore return true for no property found if allowLetConstGlobal
                    // is false.  If allowLetConstGlobal is true we will enter the else if branch below and
                    // return false since let/const variables cannot be deleted.
                    return true;
                }
                else if (!(descriptor->Attributes & PropertyConfigurable) ||
                    (allowLetConstGlobal && (descriptor->Attributes & PropertyLetConstGlobal)))
                {
                    JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, scriptContext, propertyRecord->GetBuffer());

                    return false;
                }
                Assert(!(descriptor->Attributes & PropertyLetConstGlobal));
                Var undefined = scriptContext->GetLibrary()->GetUndefined();
                if (descriptor->propertyIndex != NoSlots)
                {
                    if (SupportsSwitchingToUnordered(scriptContext))
                    {
                        ++numDeletedProperties;
                        if (numDeletedProperties >= CONFIG_FLAG(DeletedPropertyReuseThreshold))
                        {
                            // This type handler is being used as a hashtable. Start reusing deleted property indexes for new
                            // property IDs. After this, enumeration order is nondeterministic.
                            // Also use JavascriptString* as the property map key so that PropertyRecords can be avoided
                            // entirely where possible.

                            // Check if prototype chain has enumerable properties, according to logic used in
                            // ForInObjectEnumerator::Initialize().  If there are enumerable properties in the
                            // prototype chain, then enumerating this object's properties will require keeping
                            // track of properties so that shadowed properties are not included, but doing so
                            // currently requires converting the property to a PropertyRecord with a PropertyId
                            // for use in a bit vector that tracks shadowing.  To avoid having a string keyed
                            // type handler hit this, only convert to the string keyed type handler if the
                            // prototype chain does not have enumerable properties.
                            bool fConvertToStringKeyedHandler =
                                !hasNamelessPropertyId &&
                                ForInObjectEnumerator::GetFirstPrototypeWithEnumerableProperties(instance) == nullptr;

                            if (fConvertToStringKeyedHandler)
                            {
                                PHASE_PRINT_TESTTRACE1(Js::TypeHandlerTransitionPhase, L"Transitioning to string keyed SimpleDictionaryUnorderedTypeHandler\n");
                                // if TMapKey is already JavascriptString* we will not get here because we'd
                                // already be unordered and SupportsSwitchingToUnordered would have returned false
                                return ConvertToSimpleDictionaryUnorderedTypeHandler<TPropertyIndex, JavascriptString*, IsNotExtensibleSupported>(instance)
                                    ->DeleteProperty(instance, propertyId, propertyOperationFlags);
                            }
                            else
                            {
                                PHASE_PRINT_TESTTRACE1(Js::TypeHandlerTransitionPhase, L"Transitioning to PropertyRecord keyed SimpleDictionaryUnorderedTypeHandler\n");
                                return ConvertToSimpleDictionaryUnorderedTypeHandler<TPropertyIndex, TMapKey, IsNotExtensibleSupported>(instance)
                                    ->DeleteProperty(instance, propertyId, propertyOperationFlags);
                            }
                        }
                    }

                    Assert(this->singletonInstance == nullptr || instance == this->singletonInstance->Get());
                    InvalidateFixedField(propertyRecord, descriptor, instance->GetScriptContext());

                    if (this->GetFlags() & IsPrototypeFlag)
                    {
                        scriptContext->InvalidateProtoCaches(propertyId);
                    }

                    // If this is an unordered type handler, register the deleted property index so that it can be reused for
                    // other property IDs added later
                    if(!isUnordered ||
                        !AsUnordered()->TryRegisterDeletedPropertyIndex(instance, descriptor->propertyIndex))
                    {
                        SetSlotUnchecked(instance, descriptor->propertyIndex, undefined);
                    }
                }
                descriptor->Attributes = PropertyDeletedDefaults;

                // Change the type so as we can invalidate the cache in fast path jit
                instance->ChangeType();
                SetPropertyUpdateSideEffect(instance, propertyId, nullptr, SideEffects_Any);
                return true;
            }

            // Check for a numeric propertyRecord only if objectArray available
            if (instance->HasObjectArray() && propertyRecord->IsNumeric())
            {
                return SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::DeleteItem(instance, propertyRecord->IsNumeric(), propertyOperationFlags);
            }
        }
        else
        {
            return ConvertToNonSharedSimpleDictionaryType(instance)->DeleteProperty_Internal<allowLetConstGlobal>(instance, propertyId, propertyOperationFlags);
        }
        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::IsFixedProperty(const DynamicObject* instance, PropertyId propertyId)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            return descriptor->isFixed;
        }
        else
        {
            AssertMsg(false, "Asking about a property this type handler doesn't know about?");
            return false;
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if (descriptor->Attributes & PropertyLetConstGlobal)
            {
                return true;
            }
            return descriptor->Attributes & PropertyEnumerable;
        }
        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if (descriptor->Attributes & PropertyLetConstGlobal)
            {
                return !(descriptor->Attributes & PropertyConst);
            }
            return descriptor->Attributes & PropertyWritable;
        }
        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if (descriptor->Attributes & PropertyLetConstGlobal)
            {
                AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
                return true;
            }
            return descriptor->Attributes & PropertyConfigurable;
        }
        return true;
    }

    //
    // Set an attribute bit. Return true if change is made.
    //
    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetAttribute(DynamicObject* instance, SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor, PropertyAttributes attribute)
    {
        Assert(!(descriptor->Attributes & PropertyLetConstGlobal));
        if (descriptor->Attributes & PropertyDeleted)
        {
            return false;
        }

        PropertyAttributes attributes = descriptor->Attributes;
        attributes |= attribute;
        if (attributes == descriptor->Attributes)
        {
            return false;
        }

        if (GetIsLocked())
        {
            PropertyAttributes oldAttributes = descriptor->Attributes;
            descriptor->Attributes = attributes;
            ConvertToNonSharedSimpleDictionaryType(instance); // This changes TypeHandler, but non-necessarily Type.
            descriptor->Attributes = oldAttributes;
        }
        else
        {
            descriptor->Attributes = attributes;
        }
        return true;
    }

    //
    // Clear an attribute bit. Return true if change is made.
    //
    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::ClearAttribute(DynamicObject* instance, SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor, PropertyAttributes attribute)
    {
        Assert(!(descriptor->Attributes & PropertyLetConstGlobal));
        if (descriptor->Attributes & PropertyDeleted)
        {
            return false;
        }

        PropertyAttributes attributes = descriptor->Attributes;
        attributes &= ~attribute;
        if (attributes == descriptor->Attributes)
        {
            return false;
        }

        if (GetIsLocked())
        {
            PropertyAttributes oldAttributes = descriptor->Attributes;
            descriptor->Attributes = attributes;
            ConvertToNonSharedSimpleDictionaryType(instance); // This changes TypeHandler, but non-necessarily Type.
            descriptor->Attributes = oldAttributes;
        }
        else
        {
            descriptor->Attributes = attributes;
        }
        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (!propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            // Upgrade type handler if set objectArray item attribute.
            // Only check numeric propertyId if objectArray available.
            if (instance->HasObjectArray() && propertyRecord->IsNumeric())
            {
                return SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::ConvertToTypeWithItemAttributes(instance)
                    ->SetEnumerable(instance, propertyId, value);
            }
            return true;
        }

        if (descriptor->Attributes & PropertyLetConstGlobal)
        {
            return true;
        }

        if (value)
        {
            if (SetAttribute(instance, descriptor, PropertyEnumerable))
            {
                instance->SetHasNoEnumerableProperties(false);
            }
        }
        else
        {
            ClearAttribute(instance, descriptor, PropertyEnumerable);
        }
        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (!propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            // Upgrade type handler if set objectArray item attribute.
            // Only check numeric propertyId if objectArray available.
            if (instance->HasObjectArray() && propertyRecord->IsNumeric())
            {
                return SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::ConvertToTypeWithItemAttributes(instance)
                    ->SetWritable(instance, propertyId, value);
            }
            return true;
        }

        if (descriptor->Attributes & PropertyLetConstGlobal)
        {
            return true;
        }

        const Type* oldType = instance->GetType();
        if (value)
        {
            if (SetAttribute(instance, descriptor, PropertyWritable))
            {
                instance->ChangeTypeIf(oldType); // Ensure type change to invalidate caches
            }
        }
        else
        {
            if (ClearAttribute(instance, descriptor, PropertyWritable))
            {
                instance->ChangeTypeIf(oldType); // Ensure type change to invalidate caches

                // Clearing the attribute may have changed the type handler, so make sure
                // we access the current one.
                DynamicTypeHandler *const typeHandler = GetCurrentTypeHandler(instance);
                typeHandler->ClearHasOnlyWritableDataProperties();
                if(typeHandler->GetFlags() & IsPrototypeFlag)
                {
                    instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }
        }
        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (!propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            // Upgrade type handler if set objectArray item attribute.
            // Only check numeric propertyId if objectArray available.
            if (instance->HasObjectArray() && propertyRecord->IsNumeric())
            {
                return SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::ConvertToTypeWithItemAttributes(instance)
                    ->SetConfigurable(instance, propertyId, value);
            }
            return true;
        }

        if (descriptor->Attributes & PropertyLetConstGlobal)
        {
            return true;
        }

        if (value)
        {
            SetAttribute(instance, descriptor, PropertyConfigurable);
        }
        else
        {
            ClearAttribute(instance, descriptor, PropertyConfigurable);
        }
        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::PreventExtensions(DynamicObject* instance)
    {
        if (IsNotExtensibleSupported)
        {
            if ((this->GetFlags() & IsExtensibleFlag) == 0)
            {
                // Already not extensible => no further change needed.
                return TRUE;
            }

            if (!GetIsLocked())
            {
                // If the type is not shared with others, we can just change it by itself.
                return PreventExtensionsInternal(instance);
            }
        }

        return ConvertToDictionaryType(instance)->PreventExtensions(instance);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::PreventExtensionsInternal(DynamicObject* instance)
    {
        AssertMsg(IsNotExtensibleSupported, "This method must not be called for SimpleDictionaryTypeHandler<TPropertyIndex, IsNotExtensibleSupported = false>");

        this->ClearFlags(IsExtensibleFlag);

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray)
        {
            objectArray->PreventExtensions();
        }

        return TRUE;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::Seal(DynamicObject* instance)
    {
        if (IsNotExtensibleSupported)
        {
            if (this->GetFlags() & IsSealedOnceFlag)
            {
                // Already sealed => no further change needed.
                return TRUE;
            }

            if (!GetIsLocked() && !instance->HasObjectArray())
            {
                // If there is object array, we need to convert both type handler and array to ES5.
                // Otherwise, if the type is not shared with others, we can just change it by itself.
                return this->SealInternal(instance);
            }
        }

        return ConvertToDictionaryType(instance)->Seal(instance);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SealInternal(DynamicObject* instance)
    {
        AssertMsg(IsNotExtensibleSupported, "This method must not be called for SimpleDictionaryTypeHandler<TPropertyIndex, IsNotExtensibleSupported = false>");

        this->ChangeFlags(IsExtensibleFlag | IsSealedOnceFlag, 0 | IsSealedOnceFlag);

        //Set [[Configurable]] flag of each property to false
        SimpleDictionaryPropertyDescriptor<TPropertyIndex> *descriptor = nullptr;
        for (TPropertyIndex index = 0; index < propertyMap->Count(); index++)
        {
            descriptor = propertyMap->GetReferenceAt(index);
            if (!(descriptor->Attributes & PropertyLetConstGlobal))
            {
                descriptor->Attributes &= (~PropertyConfigurable);
            }
        }

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray)
        {
            objectArray->Seal();
        }

        return TRUE;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {
        if (IsNotExtensibleSupported)
        {
            if (this->GetFlags() & IsFrozenOnceFlag)
            {
                // Already frozen => no further change needed.
                return TRUE;
            }

            if (!GetIsLocked() && !instance->HasObjectArray())
            {
                // If there is object array, we need to convert both type handler and array to ES5.
                // Otherwise, if the type is not shared with others, we can just change it by itself.
                // If the type is not shared with others, we can just change it by itself.
                return FreezeInternal(instance, isConvertedType);
            }
        }

        return ConvertToDictionaryType(instance)->Freeze(instance);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::FreezeInternal(DynamicObject* instance, bool isConvertedType)
    {
        AssertMsg(IsNotExtensibleSupported, "This method must not be called for SimpleDictionaryTypeHandler<TPropertyIndex, IsNotExtensibleSupported = false>");

        this->ChangeFlags(IsExtensibleFlag | IsSealedOnceFlag | IsFrozenOnceFlag,
            0 | IsSealedOnceFlag | IsFrozenOnceFlag);

        //Set [[Writable]] flag of each property to false except for setter\getters
        //Set [[Configurable]] flag of each property to false
        SimpleDictionaryPropertyDescriptor<TPropertyIndex> *descriptor = nullptr;
        for (TPropertyIndex index = 0; index < propertyMap->Count(); index++)
        {
            descriptor = propertyMap->GetReferenceAt(index);
            if (!(descriptor->Attributes & PropertyLetConstGlobal))
            {
                descriptor->Attributes &= ~(PropertyWritable | PropertyConfigurable);
            }
        }

        if (!isConvertedType)
        {
            // Change of [[Writable]] property requires cache invalidation, hence ChangeType
            instance->ChangeType();
        }

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray)
        {
            objectArray->Freeze();
        }

        this->ClearHasOnlyWritableDataProperties();
        if (GetFlags() & IsPrototypeFlag)
        {
            InvalidateStoreFieldCachesForAllProperties(instance->GetScriptContext());
            instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }

        return TRUE;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::IsSealed(DynamicObject* instance)
    {
        if (!IsNotExtensibleSupported)
        {
            return false;
        }

        BYTE flags = this->GetFlags();
        if (flags & IsSealedOnceFlag)
        {
            // Once sealed, there is no way to undo seal.
            // But note: still, it can also be sealed when the flag is not set.
            return true;
        }

        if (flags & IsExtensibleFlag)
        {
            return false;
        }

        SimpleDictionaryPropertyDescriptor<TPropertyIndex> *descriptor = nullptr;
        for (TPropertyIndex index = 0; index < propertyMap->Count(); index++)
        {
            descriptor = propertyMap->GetReferenceAt(index);
            if ((!(descriptor->Attributes & PropertyDeleted) && !(descriptor->Attributes & PropertyLetConstGlobal)))
            {
                if (descriptor->Attributes & PropertyConfigurable)
                {
                    // [[Configurable]] must be false for all (existing) properties.
                    return false;
                }
            }
        }

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray && !objectArray->IsSealed())
        {
            return false;
        }

        // Since we've determined that the object was sealed, set the flag to avoid further checks into all properties
        // (once sealed there is no way to go back to un-sealed).
        this->SetFlags(IsSealedOnceFlag);

        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::IsFrozen(DynamicObject* instance)
    {
        if (!IsNotExtensibleSupported)
        {
            return false;
        }

        BYTE flags = this->GetFlags();
        if (flags & IsFrozenOnceFlag)
        {
            // Once frozen, there is no way to undo freeze.
            // But note: still, it can also be frozen when the flag is not set.
            return true;
        }

        if (this->GetFlags() & IsExtensibleFlag)
        {
            return false;
        }

        SimpleDictionaryPropertyDescriptor<TPropertyIndex> *descriptor = nullptr;
        for (TPropertyIndex index = 0; index < propertyMap->Count(); index++)
        {
            descriptor = propertyMap->GetReferenceAt(index);
            if ((!(descriptor->Attributes & PropertyDeleted) && !(descriptor->Attributes & PropertyLetConstGlobal)))
            {
                // [[Configurable]] and [[Configurable]] must be false for all (existing) properties.
                // IE9 compatibility: keep IE9 behavior (also check deleted properties)
                if (descriptor->Attributes & PropertyConfigurable)
                {
                    return false;
                }

                if (descriptor->Attributes & PropertyWritable)
                {
                    return false;
                }
            }
        }

        // Use IsObjectArrayFrozen() to skip "length" [[Writable]] check
        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray && !objectArray->IsObjectArrayFrozen())
        {
            return false;
        }

        // Since we've determined that the object was frozen, set the flag to avoid further checks into all properties
        // (once frozen there is no way to go back to un-frozen).
        this->SetFlags(IsSealedOnceFlag | IsFrozenOnceFlag);

        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {
        return ConvertToDictionaryType(instance)->SetAccessors(instance, propertyId, getter, setter, flags);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        AnalysisAssert(instance);
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        ScriptContext* scriptContext = instance->GetScriptContext();
        bool throwIfNotExtensible = (flags & PropertyOperation_ThrowIfNotExtensible) != 0;

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if ((attributes & PropertyLetConstGlobal) != (descriptor->Attributes & PropertyLetConstGlobal))
            {
                // We have a shadowing case here.  Either a let/const is being declared
                // that shadows an undeclared global property or an undeclared global
                // property is being added after a let/const was already declared.
                //
                // SimpleDictionaryTypeHandlerBase does not handle shadowed globals
                // because we do not want to add another property index field to the
                // property descriptors.  Instead convert to DictionaryTypeHandler
                // where it will reuse one of the getter/setter fields on its
                // property descriptor type.
                //
                // An exception is in the language service that will sometimes execute
                // the glo function twice causing a const or let appear to shadow itself.
                // In this case setting the property is also right.
                return
                    ConvertToDictionaryType(instance)
                        ->SetPropertyWithAttributes(
                                instance,
                                propertyId,
                                value,
                                attributes,
                                info,
                                flags,
                                possibleSideEffects);
            }

            if (descriptor->Attributes & PropertyDeleted && !(descriptor->Attributes & PropertyLetConstGlobal))
            {
                Assert((descriptor->Attributes & PropertyLetConstGlobal) == 0);
                if(GetIsLocked())
                {
                    return
                        ConvertToNonSharedSimpleDictionaryType(instance)
                            ->SetPropertyWithAttributes(
                                instance,
                                propertyId,
                                value,
                                attributes,
                                info,
                                flags,
                                possibleSideEffects);
                }

                if(isUnordered)
                {
                    TPropertyIndex propertyIndex;
                    if(AsUnordered()->TryUndeleteProperty(instance, descriptor->propertyIndex, &propertyIndex))
                    {
                        Assert(PropertyRecordStringHashComparer<TMapKey>::Equals(propertyMap->GetKeyAt(propertyIndex), propertyRecord));
                        descriptor = propertyMap->GetReferenceAt(propertyIndex);
                    }
                }

                if (IsNotExtensibleSupported)
                {
                    bool isForce = (flags & PropertyOperation_Force) != 0;
                    if (!isForce)
                    {
                        if (!this->VerifyIsExtensible(scriptContext, throwIfNotExtensible))
                        {
                            return FALSE;
                        }
                    }
                }

                if(SupportsSwitchingToUnordered(scriptContext))
                {
                    --numDeletedProperties;
                }
                scriptContext->InvalidateProtoCaches(propertyId);
                descriptor->Attributes = PropertyDynamicTypeDefaults;
            }

            if (descriptor->Attributes != attributes)
            {
                if (GetIsLocked())
                {
                    return
                        ConvertToNonSharedSimpleDictionaryType(instance)
                            ->SetPropertyWithAttributes(
                                instance,
                                propertyId,
                                value,
                                attributes,
                                info,
                                flags,
                                possibleSideEffects);
                }
                else
                {
                    descriptor->Attributes = attributes;
                }
            }

            if (descriptor->propertyIndex != NoSlots)
            {
                DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
                Assert(this->singletonInstance == nullptr || localSingletonInstance == instance);
                if (!descriptor->isInitialized)
                {
                    if ((flags & PropertyOperation_PreInit) == 0)
                    {
                        // Consider: It would be nice to assert the slot is actually null.  However, we sometimes pre-initialize to undefined or even
                        // some other special illegal value (for let or const), currently == null.
                        descriptor->isInitialized = true;
                        if (localSingletonInstance == instance && !IsInternalPropertyId(propertyId) && (flags & (PropertyOperation_SpecialValue | PropertyOperation_NonFixedValue)) == 0)
                        {
                            Assert(!GetIsShared());
                            Assert(value != nullptr);
                            // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
                            Assert(!instance->IsExternal());
                            descriptor->isFixed = (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : (ShouldFixDataProperties() && CheckHeuristicsForFixedDataProps(instance, propertyId, value)));
                        }
                    }
                }
                else
                {
                    InvalidateFixedField(propertyRecord, descriptor, instance->GetScriptContext());
                }

                SetSlotUnchecked(instance, descriptor->propertyIndex, value);

                if (descriptor->isInitialized && !descriptor->isFixed)
                {
                    SetPropertyValueInfo(info, instance, descriptor->propertyIndex, descriptor->Attributes);
                }
                else
                {
                    PropertyValueInfo::SetNoCache(info, instance);
                }
            }

            if (descriptor->Attributes & PropertyEnumerable)
            {
                instance->SetHasNoEnumerableProperties(false);
            }

            if (!(descriptor->Attributes & PropertyWritable))
            {
                this->ClearHasOnlyWritableDataProperties();
                if(GetFlags() & IsPrototypeFlag)
                {
                    instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }
            SetPropertyUpdateSideEffect(instance, propertyId, value, possibleSideEffects);
            return true;
        }

        // Always check numeric propertyRecord. May create objectArray.
        if (propertyRecord->IsNumeric())
        {
            return SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetItemWithAttributes(instance, propertyRecord->GetNumericValue(), value, attributes);
        }

        return this->AddProperty(instance, propertyRecord, value, attributes, info, flags, possibleSideEffects);
    }

    // We need to override SetItem as JavascriptArray (in contrary to ES5Array doesn't have checks for object being not extensible).
    // So, check here.
    // Note that we don't need to override SetItemWithAttributes because for that one base class implementation calls ConvertToTypeWithItemAttributes
    // which converts both this type and its objectArray to DictionaryTypeHandler/ES5ArrayTypeHandler.
    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetItem(DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags)
    {
        if (IsNotExtensibleSupported)
        {
            // When adding a new property && we are not extensible:
            // - if (!objectArray) => do not even get into creating new objectArrray
            //   (anyhow, if we were to create one, we would need one supporting non-extensible, i.e. ES5Array).
            // - else the array was created earlier and will handle the operation
            //   (it would be non-extensible ES5 array as array must match object's IsExtensible).
            bool isExtensible = (this->GetFlags() & IsExtensibleFlag) != 0;
            if (!isExtensible && !instance->HasObjectArray())   // Note: Setitem && !HasObjectArray => attempt to add a new item.
            {
                bool throwIfNotExtensible = (flags & (PropertyOperation_StrictMode | PropertyOperation_ThrowIfNotExtensible)) != 0;
                if (throwIfNotExtensible)
                {
                    ScriptContext* scriptContext = instance->GetScriptContext();
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NonExtensibleObject);
                }
                return FALSE;
            }
        }

        return instance->SetObjectArrayItem(index, value, flags);   // I.e. __super::SetItem(...).
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::EnsureSlotCapacity(DynamicObject * instance)
    {
        Assert(this->GetSlotCapacity() < MaxPropertyIndexSize); // Otherwise we can't grow this handler's capacity. We should've evolved to bigger handler or OOM.

        // This check should be done by caller of this function.
        //if (slotCapacity <= nextPropertyIndex)
        {
            // A Dictionary type is expected to have more properties
            // grow exponentially rather linearly to avoid the realloc and moves,
            // however use a small exponent to avoid waste
            int newSlotCapacity = (nextPropertyIndex + 1);
            newSlotCapacity += (newSlotCapacity>>2);
            if (newSlotCapacity > MaxPropertyIndexSize)
            {
                newSlotCapacity = MaxPropertyIndexSize;
            }
            newSlotCapacity = RoundUpSlotCapacity(newSlotCapacity, GetInlineSlotCapacity());
            Assert(newSlotCapacity <= MaxPropertyIndexSize);

            instance->EnsureSlots(this->GetSlotCapacity(), newSlotCapacity, instance->GetScriptContext(), this);
            this->SetSlotCapacity(newSlotCapacity);
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {
        if (!GetIsLocked())
        {
            SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
            Assert(propertyId != Constants::NoProperty);
            PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
            if (propertyMap->TryGetReference(propertyRecord, &descriptor))
            {
                if (attributes & PropertyLetConstGlobal)
                {
                    Assert(!(descriptor->Attributes & PropertyLetConstGlobal));
                    // Need to implement type transition to DictionaryTypeHandler in the case of
                    // shadowing a var or global property with a let in a new script body.
                    Throw::NotImplemented();
                }
                if (descriptor->Attributes & PropertyLetConstGlobal)
                {
                    Assert(!(attributes & PropertyLetConstGlobal));
                    Assert(false);
                }

                if (descriptor->Attributes & PropertyDeleted)
                {
                    return false;
                }

                descriptor->Attributes = (descriptor->Attributes & ~PropertyDynamicTypeDefaults) | (attributes & PropertyDynamicTypeDefaults);

                if (attributes & PropertyEnumerable)
                {
                    instance->SetHasNoEnumerableProperties(false);
                }

                if (!(descriptor->Attributes & PropertyWritable))
                {
                    this->ClearHasOnlyWritableDataProperties();
                    if(GetFlags() & IsPrototypeFlag)
                    {
                        instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyId);
                        instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                    }
                }

                return true;
            }

            // Check numeric propertyRecord only if objectArray available
            if (instance->HasObjectArray() && propertyRecord->IsNumeric())
            {
                return SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetItemAttributes(instance, propertyRecord->GetNumericValue(), attributes);
            }
        }
        else
        {
            return ConvertToNonSharedSimpleDictionaryType(instance)->SetAttributes(instance, propertyId, attributes);
        }
        return false;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {
        // this might get value that are deleted from the dictionary, but that should be nulled out
        SimpleDictionaryPropertyDescriptor<TPropertyIndex> const * descriptor;
        if (!propertyMap->TryGetValueAt(index, &descriptor))
        {
            return false;
        }
        Assert(descriptor->propertyIndex == index);
        if (descriptor->Attributes & PropertyDeleted)
        {
            return false;
        }
        *attributes = descriptor->Attributes & PropertyDynamicTypeDefaults;
        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <typename TPropertyKey>
    BOOL SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::AddProperty(DynamicObject* instance, TPropertyKey propertyKey, Var value, PropertyAttributes attributes,
        PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
#if DBG
        // Only Assert that the propertyMap doesn't contain propertyKey if TMapKey is string and propertyKey is not a symbol.
        if (!TMapKey_IsJavascriptString<TMapKey>() || !TMapKey_IsSymbol(propertyKey, scriptContext))
        {
            SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
            Assert(!propertyMap->TryGetReference(propertyKey, &descriptor));
        }
        Assert(!TPropertyKey_IsNumeric(propertyKey));
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (CONFIG_FLAG(ForceStringKeyedSimpleDictionaryTypeHandler) &&
            !TMapKey_IsJavascriptString<TMapKey>() &&
            !isUnordered && !hasNamelessPropertyId &&
            !TPropertyKey_IsInternalPropertyId(propertyKey) &&
            !TMapKey_IsSymbol(propertyKey, scriptContext))
        {
            return ConvertToSimpleDictionaryUnorderedTypeHandler<TPropertyIndex, JavascriptString*, IsNotExtensibleSupported>(instance)
                ->AddProperty(instance, propertyKey, value, attributes, info, flags, possibleSideEffects);
        }
#endif

        if (IsNotExtensibleSupported)
        {
            bool isForce = (flags & PropertyOperation_Force) != 0;
            if (!isForce)
            {
                bool throwIfNotExtensible = (flags & (PropertyOperation_ThrowIfNotExtensible | PropertyOperation_StrictMode)) != 0;
                if (!this->VerifyIsExtensible(scriptContext, throwIfNotExtensible))
                {
                    return FALSE;
                }
            }
        }

        SimpleDictionaryTypeHandlerBase * typeHandler = this;
        if (GetIsLocked())
        {
            typeHandler = ConvertToNonSharedSimpleDictionaryType(instance);
        }

        if (TMapKey_IsJavascriptString<TMapKey>() &&
            (TPropertyKey_IsInternalPropertyId(propertyKey) || TMapKey_IsSymbol(propertyKey, scriptContext)))
        {
            PHASE_PRINT_TESTTRACE1(Js::TypeHandlerTransitionPhase, L"Transitioning from string keyed to PropertyRecord keyed SimpleDictionaryUnorderedTypeHandler\n");
            // String keyed type handler cannot store InternalPropertyRecords since they have no string representation
            return ConvertToSimpleDictionaryUnorderedTypeHandler<TPropertyIndex, const PropertyRecord*, IsNotExtensibleSupported>(instance)
                ->AddProperty(instance, propertyKey, value, attributes, info, flags, possibleSideEffects);
        }

        if (this->GetSlotCapacity() <= nextPropertyIndex)
        {
            if (this->GetSlotCapacity() >= MaxPropertyIndexSize)
            {
                BigSimpleDictionaryTypeHandler* newTypeHandler = ConvertToBigSimpleDictionaryTypeHandler(instance);

                return newTypeHandler->AddProperty(instance, propertyKey, value, attributes, info, flags, possibleSideEffects);
            }

            typeHandler->EnsureSlotCapacity(instance);
        }

        Assert((flags & PropertyOperation_SpecialValue) != 0 || value != nullptr);
        Assert(!typeHandler->GetIsShared());
        Assert(typeHandler->singletonInstance == nullptr || typeHandler->singletonInstance->Get() == instance);
        bool markAsInitialized = ((flags & PropertyOperation_PreInit) == 0);
        bool markAsFixed = markAsInitialized && !TPropertyKey_IsInternalPropertyId(propertyKey) && (flags & (PropertyOperation_NonFixedValue | PropertyOperation_SpecialValue)) == 0 &&
            typeHandler->singletonInstance != nullptr && typeHandler->singletonInstance->Get() == instance
            && (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : (ShouldFixDataProperties() && CheckHeuristicsForFixedDataProps(instance, propertyKey, value)));

        TPropertyIndex index;
        if (typeHandler->isUnordered &&
            typeHandler->AsUnordered()->TryReuseDeletedPropertyIndex(instance, &index))
        {
            // We are reusing a deleted property index for the new property ID. Update the property map.
            Assert(typeHandler->propertyMap->GetValueAt(index).propertyIndex == index);
            TMapKey deletedPropertyKey = typeHandler->propertyMap->GetKeyAt(index);
            typeHandler->propertyMap->Remove(deletedPropertyKey);
            typeHandler->Add(index, propertyKey, attributes, markAsInitialized, markAsFixed, false, scriptContext);
        }
        else
        {
            index = nextPropertyIndex;
            typeHandler->Add(propertyKey, attributes, markAsInitialized, markAsFixed, false, scriptContext);
        }

        if (attributes & PropertyEnumerable)
        {
            instance->SetHasNoEnumerableProperties(false);
        }

        SetSlotUnchecked(instance, index, value);

        // It's ok to populate inline cache here even if this handler isn't shared yet, because we don't have property add
        // inline cache flavor for SimpleDictionaryTypeHandlers.  This ensures that a) no new instance reaches this handler
        // without us knowing, and b) the inline cache doesn't inadvertently become polymorphic.
        if (markAsInitialized && !markAsFixed)
        {
            SetPropertyValueInfo(info, instance, index, attributes);
        }
        else
        {
            PropertyValueInfo::SetNoCache(info, instance);
        }

        PropertyId propertyId = TPropertyKey_GetOptionalPropertyId(scriptContext, propertyKey);
        if (propertyId != Constants::NoProperty)
        {
            if ((typeHandler->GetFlags() & IsPrototypeFlag)
                || (!IsInternalPropertyId(propertyId)
                && JavascriptOperators::HasProxyOrPrototypeInlineCacheProperty(instance, propertyId)))
            {
                // We don't evolve dictionary types when adding a field, so we need to invalidate prototype caches.
                // We only have to do this though if the current type is used as a prototype, or the current property
                // is found on the prototype chain.
                scriptContext->InvalidateProtoCaches(propertyId);
            }
            SetPropertyUpdateSideEffect(instance, propertyId, value, possibleSideEffects);
        }
        return true;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetAllPropertiesToUndefined(DynamicObject* instance, bool invalidateFixedFields)
    {
        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.
        int propertyCount = this->propertyMap->Count();
        if (IsNotExtensibleSupported)
        {
            // The Var for window is reused across navigation. we shouldn't preserve the IsExtensibleFlag when we don't keep
            // the expandoes. Reset the IsExtensibleFlag in cleanup scenario should be good enough
            // to cover all the preventExtension/Freeze/Seal scenarios.
            ChangeFlags(IsExtensibleFlag | IsSealedOnceFlag | IsFrozenOnceFlag, IsExtensibleFlag);
        }

        if (invalidateFixedFields)
        {
            Js::ScriptContext* scriptContext = instance->GetScriptContext();
            for (int propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
            {
                const TMapKey propertyRecord = this->propertyMap->GetKeyAt(propertyIndex);
                SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor = this->propertyMap->GetReferenceAt(propertyIndex);
                InvalidateFixedField(propertyRecord, descriptor, scriptContext);
            }
        }

        Js::RecyclableObject* undefined = instance->GetLibrary()->GetUndefined();
        int slotCount = this->nextPropertyIndex;
        for (int slotIndex = 0; slotIndex < slotCount; slotIndex++)
        {
            SetSlotUnchecked(instance, slotIndex, undefined);
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::MarshalAllPropertiesToScriptContext(DynamicObject* instance, ScriptContext* targetScriptContext, bool invalidateFixedFields)
    {
        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.
        int propertyCount = this->propertyMap->Count();

        if (invalidateFixedFields)
        {
            ScriptContext* scriptContext = instance->GetScriptContext();
            for (int propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
            {
                const TMapKey propertyRecord = this->propertyMap->GetKeyAt(propertyIndex);
                SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor = this->propertyMap->GetReferenceAt(propertyIndex);
                InvalidateFixedField(propertyRecord, descriptor, scriptContext);
            }
        }

        int slotCount = this->nextPropertyIndex;
        for (int slotIndex = 0; slotIndex < slotCount; slotIndex++)
        {
            SetSlotUnchecked(instance, slotIndex, CrossSite::MarshalVar(targetScriptContext, GetSlot(instance, slotIndex)));
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    DynamicTypeHandler* SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::ConvertToTypeWithItemAttributes(DynamicObject* instance)
    {
        return JavascriptArray::Is(instance) ?
            ConvertToES5ArrayType(instance) : ConvertToDictionaryType(instance);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetIsPrototype(DynamicObject* instance)
    {
        // Don't return if IsPrototypeFlag is set, because we may still need to do a type transition and
        // set fixed bits.  If this handler is shared, this instance may not even be a prototype yet.
        // In this case we may need to convert to a non-shared type handler.
        if (!ChangeTypeOnProto() && !(GetIsOrMayBecomeShared() && IsolatePrototypes()))
        {
            SetFlags(IsPrototypeFlag);
            return;
        }

        Assert(!GetIsShared() || this->singletonInstance == nullptr);
        Assert(this->singletonInstance == nullptr || this->singletonInstance->Get() == instance);

        SetIsPrototype(instance, false);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetIsPrototype(DynamicObject* instance, bool hasNewType)
    {
        const auto setFixedFlags = [instance](TMapKey propertyKey, SimpleDictionaryPropertyDescriptor<TPropertyIndex>* const descriptor, bool hasNewType)
        {
            if (TPropertyKey_IsInternalPropertyId(propertyKey))
            {
                return;
            }
            if (!(descriptor->Attributes & PropertyDeleted) && !(descriptor->Attributes & PropertyLetConstGlobal))
            {
                // See PathTypeHandlerBase::ConvertToSimpleDictionaryType for rules governing fixed field bits during type
                // handler transitions.  In addition, we know that the current instance is not yet a prototype.
                if (descriptor->propertyIndex != NoSlots)
                {
                    // Consider: If we decide to fix all types of properties, we could skip loading the value from the instance.
                    if (descriptor->isInitialized)
                    {
                        Var value = instance->GetSlot(descriptor->propertyIndex);
                        // Even though the handler says the property is initialized the particular instance may not yet have
                        // a value for this property.  This should only happen if the handler is shared.
                        if (value != nullptr)
                        {
                            if (hasNewType)
                            {
                                // Since we have a non-shared type handler, it's ok to fix all fields at their current values, as long as
                                // we've also taken a type transition.  Otherwise populated load field caches would still be valid and
                                // would need to be explicitly invalidated if the property value changes.

                                // saravind:If the instance is used by a CrossSiteObject, then we are conservative and do not mark any field as fixed in that instance.
                                // We need to relax this in the future and support fixed fields for Cross Site Context usage
                                descriptor->isFixed = (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : (ShouldFixDataProperties() && CheckHeuristicsForFixedDataProps(instance, propertyKey, value)));

                                // Since we have a new type we can clear all used as fixed bits.  That's because any instance field loads
                                // will have been invalidated by the type transition, and there are no proto fields loads from this object
                                // because it is just now becoming a proto.
                                descriptor->usedAsFixed = false;
                            }
                        }
                    }
                    else
                    {
                        Assert(!descriptor->isFixed && !descriptor->usedAsFixed);
                    }
                }
            }
        };

        bool isShared = GetIsShared();
        if (GetIsOrMayBecomeShared() && IsolatePrototypes())
        {
            Type* oldType = instance->GetType();
            ConvertToNonSharedSimpleDictionaryType(instance)->SetIsPrototype(instance, instance->GetType() != oldType);
        }
        else
        {

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            DynamicType* oldType = instance->GetDynamicType();
            RecyclerWeakReference<DynamicObject>* oldSingletonInstance = GetSingletonInstance();
            TraceFixedFieldsBeforeSetIsProto(instance, this, oldType, oldSingletonInstance);
#endif

            if (!hasNewType && ChangeTypeOnProto())
            {
                // We're about to split out the type.  If the original type was shared the handler better be shared as well.
                // Otherwise, the handler would lose track of being shared between different types and instances.
                Assert(!instance->HasSharedType() || instance->GetDynamicType()->GetTypeHandler()->GetIsShared());
                // Forcing a type transition allows us to fix all fields (even those that were previously marked as non-fixed).
                instance->ChangeType();
                Assert(!instance->HasSharedType());
                hasNewType = true;
            }

            if (!isShared)
            {
                Assert(this->singletonInstance == nullptr || this->singletonInstance->Get() == instance);
                Assert(HasSingletonInstanceOnlyIfNeeded());
                if (AreSingletonInstancesNeeded() && this->singletonInstance == nullptr)
                {
                    this->singletonInstance = instance->CreateWeakReferenceToSelf();
                }

                // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
                if (!instance->IsExternal())
                {
                    // If this type handler is not shared by any types or instances we can simply mark all existing properties as fixed.
                    // The propertyMap dictionary is guaranteed to have contiguous entries because we never remove entries from it.
                    for (int i = 0; i < propertyMap->Count(); i++)
                    {
                        TMapKey propertyKey = propertyMap->GetKeyAt(i);
                        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* const descriptor = propertyMap->GetReferenceAt(i);
                        setFixedFlags(propertyKey, descriptor, hasNewType);
                    }
                }
            }

            SetFlags(IsPrototypeFlag);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            TraceFixedFieldsAfterSetIsProto(instance, this, this, oldType, oldSingletonInstance);
#endif

        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::HasSingletonInstance() const
    {
        return this->singletonInstance != nullptr;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::TryUseFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext)
    {
        bool result = TryGetFixedProperty<false, true>(propertyRecord, pProperty, propertyType, requestContext);
        TraceUseFixedProperty(propertyRecord, pProperty, result, L"SimpleDictionaryTypeHandler", requestContext);
        return result;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::TryUseFixedAccessor(PropertyRecord const * propertyRecord, Var * pAccessor, FixedPropertyKind propertyType, bool getter, ScriptContext * requestContext)
    {
        if (PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase) ||
            PHASE_VERBOSE_TRACE1(Js::UseFixedDataPropsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::UseFixedDataPropsPhase))
        {
            Output::Print(L"FixedFields: attempt to use fixed accessor %s from SimpleDictionaryTypeHandler returned false.\n", propertyRecord->GetBuffer());
            if (this->HasSingletonInstance() && this->GetSingletonInstance()->Get()->GetScriptContext() != requestContext)
            {
                Output::Print(L"FixedFields: Cross Site Script Context is used for property %s. \n", propertyRecord->GetBuffer());
            }
            Output::Flush();
        }
        return false;
    }

#if DBG
    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::CanStorePropertyValueDirectly(const DynamicObject* instance, PropertyId propertyId, bool allowLetConst)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;

        // We pass Constants::NoProperty for ActivationObjects for functions with same named formals.
        if (propertyId == Constants::NoProperty)
        {
            return true;
        }

        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if (allowLetConst && (descriptor->Attributes & PropertyLetConstGlobal))
            {
                return true;
            }
            else
            {
                AssertMsg(!(descriptor->Attributes & PropertyLetConstGlobal), "Asking about a global property this type handler doesn't have?");
                return descriptor->isInitialized && !descriptor->isFixed;
            }
        }
        else
        {
            AssertMsg(false, "Asking about a property this type handler doesn't know about?");
            return false;
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::CheckFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, ScriptContext * requestContext)
    {
        return TryGetFixedProperty<true, false>(propertyRecord, pProperty, (FixedPropertyKind) (Js::FixedPropertyKind::FixedMethodProperty | Js::FixedPropertyKind::FixedDataProperty), requestContext);
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::HasAnyFixedProperties() const
    {
        for (int i = 0; i < propertyMap->Count(); i++)
        {
            SimpleDictionaryPropertyDescriptor<TPropertyIndex> descriptor = propertyMap->GetValueAt(i);
            if (descriptor.isFixed)
            {
                return true;
            }
        }
        return false;
    }
#endif

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <bool allowNonExistent, bool markAsUsed>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::TryGetFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext)
    {
        // Note: This function is not thread-safe and cannot be called from the JIT thread.  That's why we collect and
        // cache any fixed function instances during work item creation on the main thread.
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        if (localSingletonInstance != nullptr && localSingletonInstance->GetScriptContext() == requestContext)
        {
            SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
            if (propertyMap->TryGetReference(propertyRecord, &descriptor))
            {
                if (descriptor->isFixed)
                {
                    AssertMsg(!(descriptor->Attributes & PropertyLetConstGlobal), "can't have fixed global let/const");
                    Assert(!IsInternalPropertyId(propertyRecord->GetPropertyId()));
                    Var value = localSingletonInstance->GetSlot(descriptor->propertyIndex);
                    if (value && ((IsFixedMethodProperty(propertyType) && JavascriptFunction::Is(value)) || IsFixedDataProperty(propertyType)))
                    {
                        *pProperty = value;
                        if (markAsUsed)
                        {
                            descriptor->usedAsFixed = true;
                        }
                        return true;
                    }
                }
            }
            else
            {
                // If we're unordered, we may have removed this descriptor from the map and replaced it with a descriptor for a
                // different property. When we do that, we change the type of the instance, but the old type (which may still be
                // in some inline cache) still points to the same type handler.
                AssertMsg(allowNonExistent || isUnordered, "Trying to get a fixed function instance for a non-existent property?");
            }
        }

        return false;
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    template <typename TPropertyKey>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::InvalidateFixedField(const TPropertyKey propertyKey, SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor, ScriptContext* scriptContext)
    {
        Assert(descriptor->isInitialized);

        descriptor->isFixed = false;

        if (descriptor->usedAsFixed)
        {
#if ENABLE_NATIVE_CODEGEN
            PropertyId propertyId = TMapKey_GetPropertyId(scriptContext, propertyKey);
            scriptContext->GetThreadContext()->InvalidatePropertyGuards(propertyId);
#endif
            descriptor->usedAsFixed = false;
        }
    }

#if DBG
    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::IsLetConstGlobal(DynamicObject* instance, PropertyId propertyId)
    {
        SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor;
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor) && (descriptor->Attributes & PropertyLetConstGlobal))
        {
            return true;
        }
        return false;
    }
#endif

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    bool SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::NextLetConstGlobal(int& index, RootObjectBase* instance, const PropertyRecord** propertyRecord, Var* value, bool* isConst)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        for (; index < propertyMap->Count(); index++)
        {
            SimpleDictionaryPropertyDescriptor<TPropertyIndex> descriptor = propertyMap->GetValueAt(index);

            if (descriptor.Attributes & PropertyLetConstGlobal)
            {
                *propertyRecord = TMapKey_ConvertKey<const PropertyRecord*>(scriptContext, propertyMap->GetKeyAt(index));
                *value = instance->GetSlot(descriptor.propertyIndex);
                *isConst = (descriptor.Attributes & PropertyConst) != 0;

                index += 1;

                return true;
            }
        }

        return false;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::DumpFixedFields() const {
        for (int i = 0; i < propertyMap->Count(); i++)
        {
            SimpleDictionaryPropertyDescriptor<TPropertyIndex> descriptor = propertyMap->GetValueAt(i);
            TMapKey propertyKey = propertyMap->GetKeyAt(i);

            Output::Print(L" %s %d%d%d,", TMapKey_GetBuffer(propertyKey),
                descriptor.isInitialized ? 1 : 0, descriptor.isFixed ? 1 : 0, descriptor.usedAsFixed ? 1 : 0);
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::TraceFixedFieldsBeforeTypeHandlerChange(
        const wchar_t* oldTypeHandlerName, const wchar_t* newTypeHandlerName,
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler,
        DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
    {
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {
            Output::Print(L"FixedFields: converting 0x%p from %s to %s:\n", instance, oldTypeHandlerName, newTypeHandlerName);
            Output::Print(L"   before: type = 0x%p, type handler = 0x%p, old singleton = 0x%p(0x%p)\n",
                oldType, oldTypeHandler, oldSingletonInstanceBefore, oldSingletonInstanceBefore != nullptr ? oldSingletonInstanceBefore->Get() : nullptr);
            Output::Print(L"   fixed fields:");
            oldTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
        }
        if (PHASE_VERBOSE_TESTTRACE1(FixMethodPropsPhase))
        {
            Output::Print(L"FixedFields: converting instance from %s to %s:\n", oldTypeHandlerName, newTypeHandlerName);
            Output::Print(L"   old singleton before %s null \n", oldSingletonInstanceBefore == nullptr ? L"==" : L"!=");
            Output::Print(L"   fixed fields before:");
            oldTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::TraceFixedFieldsAfterTypeHandlerChange(
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicTypeHandler* newTypeHandler,
        DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
    {
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {
            RecyclerWeakReference<DynamicObject>* oldSingletonInstanceAfter = oldTypeHandler->GetSingletonInstance();
            RecyclerWeakReference<DynamicObject>* newSingletonInstanceAfter = newTypeHandler->GetSingletonInstance();
            Output::Print(L"   after: type = 0x%p, type handler = 0x%p, old singleton = 0x%p(0x%p), new singleton = 0x%p(0x%p)\n",
                instance->GetType(), newTypeHandler, oldSingletonInstanceAfter, oldSingletonInstanceAfter != nullptr ? oldSingletonInstanceAfter->Get() : nullptr,
                newSingletonInstanceAfter, newSingletonInstanceAfter != nullptr ? newSingletonInstanceAfter->Get() : nullptr);
            Output::Print(L"   fixed fields:");
            newTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
            Output::Flush();
        }
        if (PHASE_VERBOSE_TESTTRACE1(FixMethodPropsPhase))
        {
            Output::Print(L"   type %s, typeHandler %s, old singleton after %s null (%s), new singleton after %s null\n",
                oldTypeHandler != newTypeHandler ? L"changed" : L"unchanged",
                oldType != instance->GetType() ? L"changed" : L"unchanged",
                oldTypeHandler->GetSingletonInstance() == nullptr ? L"==" : L"!=",
                oldSingletonInstanceBefore != oldTypeHandler->GetSingletonInstance() ? L"changed" : L"unchanged",
                newTypeHandler->GetSingletonInstance() == nullptr ? L"==" : L"!=");
            Output::Print(L"   fixed fields after:");
            newTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
            Output::Flush();
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::TraceFixedFieldsBeforeSetIsProto(
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
    {
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {
            Output::Print(L"FixedFields: PathTypeHandler::SetIsPrototype(0x%p):\n", instance);
            Output::Print(L"   before: type = 0x%p, old singleton: 0x%p(0x%p)\n",
                oldType, oldSingletonInstanceBefore, oldSingletonInstanceBefore != nullptr ? oldSingletonInstanceBefore->Get() : nullptr);
            Output::Print(L"   fixed fields:");
            oldTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
        }
        if (PHASE_VERBOSE_TESTTRACE1(FixMethodPropsPhase))
        {
            Output::Print(L"FixedFields: PathTypeHandler::SetIsPrototype():\n");
            Output::Print(L"   old singleton before %s null \n", oldSingletonInstanceBefore == nullptr ? L"==" : L"!=");
            Output::Print(L"   fixed fields before:");
            oldTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
        }
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::TraceFixedFieldsAfterSetIsProto(
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicTypeHandler* newTypeHandler,
        DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
    {
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {
            RecyclerWeakReference<DynamicObject>* oldSingletonInstanceAfter = oldTypeHandler->GetSingletonInstance();
            RecyclerWeakReference<DynamicObject>* newSingletonInstanceAfter = newTypeHandler->GetSingletonInstance();
            Output::Print(L"   after: type = 0x%p, type handler = 0x%p, old singleton = 0x%p(0x%p), new singleton = 0x%p(0x%p)\n",
                instance->GetType(), newTypeHandler,
                oldSingletonInstanceAfter, oldSingletonInstanceAfter != nullptr ? oldSingletonInstanceAfter->Get() : nullptr,
                newSingletonInstanceAfter, newSingletonInstanceAfter != nullptr ? newSingletonInstanceAfter->Get() : nullptr);
            Output::Print(L"   fixed fields:");
            newTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
            Output::Flush();
        }
        if (PHASE_VERBOSE_TESTTRACE1(FixMethodPropsPhase))
        {
            Output::Print(L"   type %s, old singleton after %s null (%s)\n",
                oldType != instance->GetType() ? L"changed" : L"unchanged",
                oldSingletonInstanceBefore == nullptr ? L"==" : L"!=",
                oldSingletonInstanceBefore != oldTypeHandler->GetSingletonInstance() ? L"changed" : L"unchanged");
            Output::Print(L"   fixed fields after:");
            newTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
            Output::Flush();
        }
    }
#endif

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    typename SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::BigSimpleDictionaryTypeHandler* SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::ConvertToBigSimpleDictionaryTypeHandler(DynamicObject* instance)
    {
        return
            isUnordered
                ? ConvertToSimpleDictionaryUnorderedTypeHandler<BigPropertyIndex, TMapKey, false>(instance)
                : ConvertToTypeHandler<BigSimpleDictionaryTypeHandler, TMapKey>(instance);
    }

    template <>
    BigSimpleDictionaryTypeHandler* SimpleDictionaryTypeHandlerBase<BigPropertyIndex, const PropertyRecord*, false>::ConvertToBigSimpleDictionaryTypeHandler(DynamicObject* instance)
    {
        Throw::OutOfMemory();
    }
    template <>
    BigSimpleDictionaryTypeHandler* SimpleDictionaryTypeHandlerBase<BigPropertyIndex, const PropertyRecord*, true>::ConvertToBigSimpleDictionaryTypeHandler(DynamicObject* instance)
    {
        Throw::OutOfMemory();
    }

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    void SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>::SetPropertyValueInfo(PropertyValueInfo* info, RecyclableObject* instance, TPropertyIndex propIndex, PropertyAttributes attributes, InlineCacheFlags flags)
    {
        PropertyValueInfo::Set(info, instance, propIndex, attributes, flags);
    }

    template <>
    void SimpleDictionaryTypeHandlerBase<BigPropertyIndex, const PropertyRecord*, false>::SetPropertyValueInfo(PropertyValueInfo* info, RecyclableObject* instance, BigPropertyIndex propIndex, PropertyAttributes attributes, InlineCacheFlags flags)
    {
        PropertyValueInfo::SetNoCache(info, instance);
    }
    template <>
    void SimpleDictionaryTypeHandlerBase<BigPropertyIndex, const PropertyRecord*, true>::SetPropertyValueInfo(PropertyValueInfo* info, RecyclableObject* instance, BigPropertyIndex propIndex, PropertyAttributes attributes, InlineCacheFlags flags)
    {
        PropertyValueInfo::SetNoCache(info, instance);
    }
    template <>
    void SimpleDictionaryTypeHandlerBase<BigPropertyIndex, JavascriptString*, false>::SetPropertyValueInfo(PropertyValueInfo* info, RecyclableObject* instance, BigPropertyIndex propIndex, PropertyAttributes attributes, InlineCacheFlags flags)
    {
        PropertyValueInfo::SetNoCache(info, instance);
    }
    template <>
    void SimpleDictionaryTypeHandlerBase<BigPropertyIndex, JavascriptString*, true>::SetPropertyValueInfo(PropertyValueInfo* info, RecyclableObject* instance, BigPropertyIndex propIndex, PropertyAttributes attributes, InlineCacheFlags flags)
    {
        PropertyValueInfo::SetNoCache(info, instance);
    }

    template class SimpleDictionaryTypeHandlerBase<PropertyIndex, const PropertyRecord*, false>;
    template class SimpleDictionaryTypeHandlerBase<PropertyIndex, const PropertyRecord*, true>;
    template class SimpleDictionaryTypeHandlerBase<BigPropertyIndex, const PropertyRecord*, false>;
    template class SimpleDictionaryTypeHandlerBase<BigPropertyIndex, const PropertyRecord*, true>;

    template class SimpleDictionaryTypeHandlerBase<PropertyIndex, JavascriptString*, false>;
    template class SimpleDictionaryTypeHandlerBase<PropertyIndex, JavascriptString*, true>;
    template class SimpleDictionaryTypeHandlerBase<BigPropertyIndex, JavascriptString*, false>;
    template class SimpleDictionaryTypeHandlerBase<BigPropertyIndex, JavascriptString*, true>;
}

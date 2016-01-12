//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    template <typename T>
    DictionaryTypeHandlerBase<T>* DictionaryTypeHandlerBase<T>::New(Recycler * recycler, int initialCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots)
    {
        return NewTypeHandler<DictionaryTypeHandlerBase>(recycler, initialCapacity, inlineSlotCapacity, offsetOfInlineSlots);
    }

    template <typename T>
    DictionaryTypeHandlerBase<T>::DictionaryTypeHandlerBase(Recycler* recycler) :
        DynamicTypeHandler(1),
        nextPropertyIndex(0),
        singletonInstance(nullptr)
    {
        SetIsInlineSlotCapacityLocked();
        propertyMap = RecyclerNew(recycler, PropertyDescriptorMap, recycler, this->GetSlotCapacity());
    }

    template <typename T>
    DictionaryTypeHandlerBase<T>::DictionaryTypeHandlerBase(Recycler* recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots) :
        // Do not RoundUp passed in slotCapacity. This may be called by ConvertTypeHandler for an existing DynamicObject and should use the real existing slotCapacity.
        DynamicTypeHandler(slotCapacity, inlineSlotCapacity, offsetOfInlineSlots),
        nextPropertyIndex(0),
        singletonInstance(nullptr)
    {
        SetIsInlineSlotCapacityLocked();
        Assert(GetSlotCapacity() <= MaxPropertyIndexSize);
        propertyMap = RecyclerNew(recycler, PropertyDescriptorMap, recycler, slotCapacity);
    }

    //
    // Takes over a given dictionary typeHandler. Used only by subclass.
    //
    template <typename T>
    DictionaryTypeHandlerBase<T>::DictionaryTypeHandlerBase(DictionaryTypeHandlerBase* typeHandler) :
        DynamicTypeHandler(typeHandler->GetSlotCapacity(), typeHandler->GetInlineSlotCapacity(), typeHandler->GetOffsetOfInlineSlots()),
        propertyMap(typeHandler->propertyMap), nextPropertyIndex(typeHandler->nextPropertyIndex),
        singletonInstance(typeHandler->singletonInstance)
    {
        Assert(typeHandler->GetIsInlineSlotCapacityLocked());
        CopyPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection | PropertyTypesInlineSlotCapacityLocked, typeHandler->GetPropertyTypes());
    }

    template <typename T>
    int DictionaryTypeHandlerBase<T>::GetPropertyCount()
    {
        return propertyMap->Count();
    }

    template <typename T>
    PropertyId DictionaryTypeHandlerBase<T>::GetPropertyId(ScriptContext* scriptContext, PropertyIndex index)
    {
        if (index < propertyMap->Count())
        {
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(index);
            if (!(descriptor.Attributes & PropertyDeleted) && descriptor.HasNonLetConstGlobal())
            {
                return propertyMap->GetKeyAt(index)->GetPropertyId();
            }
        }
        return Constants::NoProperty;
    }

    template <typename T>
    PropertyId DictionaryTypeHandlerBase<T>::GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index)
    {
        if (index < propertyMap->Count())
        {
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(index);
            if (!(descriptor.Attributes & PropertyDeleted) && descriptor.HasNonLetConstGlobal())
            {
                return propertyMap->GetKeyAt(index)->GetPropertyId();
            }
        }
        return Constants::NoProperty;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyStringName,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        Assert(propertyStringName);
        Assert(propertyId);
        Assert(type);

        for(; index < propertyMap->Count(); ++index )
        {
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(index);
            PropertyAttributes attribs = descriptor.Attributes;

            if (!(attribs & PropertyDeleted) && (!requireEnumerable || (attribs & PropertyEnumerable)) &&
                (!(attribs & PropertyLetConstGlobal) || descriptor.HasNonLetConstGlobal()))
            {
                const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(index);

                // Skip this property if it is a symbol and we are not including symbol properties
                if (!enumSymbols && propertyRecord->IsSymbol())
                {
                    continue;
                }

                // Pass back attributes of this property so caller can use them if it needs
                if (attributes != nullptr)
                {
                    *attributes = attribs;
                }

                *propertyId = propertyRecord->GetPropertyId();
                PropertyString* propertyString = type->GetScriptContext()->GetPropertyString(*propertyId);
                *propertyStringName = propertyString;
                T dataSlot = descriptor.GetDataPropertyIndex<false>();
                if (dataSlot != NoSlots && (attribs & PropertyWritable))
                {
                    uint16 inlineOrAuxSlotIndex;
                    bool isInlineSlot;
                    PropertyIndexToInlineOrAuxSlotIndex(dataSlot, &inlineOrAuxSlotIndex, &isInlineSlot);

                    propertyString->UpdateCache(type, inlineOrAuxSlotIndex, isInlineSlot, descriptor.IsInitialized && !descriptor.IsFixed);
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

    template <>
    BOOL DictionaryTypeHandlerBase<BigPropertyIndex>::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyString,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        Assert(false);
        Throw::InternalError();
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyString,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        PropertyIndex local = (PropertyIndex)index;
        Assert(index <= Constants::UShortMaxValue || index == Constants::NoBigSlot);
        BOOL result = this->FindNextProperty(scriptContext, local, propertyString, propertyId, attributes, type, typeToEnumerate, requireEnumerable, enumSymbols);
        index = local;
        return result;
    }

    template <>
    BOOL DictionaryTypeHandlerBase<BigPropertyIndex>::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyStringName,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        Assert(propertyStringName);
        Assert(propertyId);
        Assert(type);

        for(; index < propertyMap->Count(); ++index )
        {
            DictionaryPropertyDescriptor<BigPropertyIndex> descriptor = propertyMap->GetValueAt(index);
            PropertyAttributes attribs = descriptor.Attributes;
            if (!(attribs & PropertyDeleted) && (!requireEnumerable || (attribs & PropertyEnumerable)) &&
                (!(attribs & PropertyLetConstGlobal) || descriptor.HasNonLetConstGlobal()))
            {
                const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(index);

                // Skip this property if it is a symbol and we are not including symbol properties
                if (!enumSymbols && propertyRecord->IsSymbol())
                {
                    continue;
                }

                if (attributes != nullptr)
                {
                    *attributes = attribs;
                }

                *propertyId = propertyRecord->GetPropertyId();
                *propertyStringName = type->GetScriptContext()->GetPropertyString(*propertyId);

                return TRUE;
            }
        }

        return FALSE;
    }

    template <typename T>
    PropertyIndex DictionaryTypeHandlerBase<T>::GetPropertyIndex(PropertyRecord const* propertyRecord)
    {
        return GetPropertyIndex_Internal<false>(propertyRecord);
    }

    template <typename T>
    PropertyIndex DictionaryTypeHandlerBase<T>::GetRootPropertyIndex(PropertyRecord const* propertyRecord)
    {
        return GetPropertyIndex_Internal<true>(propertyRecord);
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        if (this->propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {
            AssertMsg(descriptor->GetDataPropertyIndex<false>() != Constants::NoSlot, "We don't support equivalent object type spec on accessors.");
            AssertMsg(descriptor->GetDataPropertyIndex<false>() <= Constants::PropertyIndexMax, "We don't support equivalent object type spec on big property indexes.");
            T propertyIndex = descriptor->GetDataPropertyIndex<false>();
            info.slotIndex = propertyIndex <= Constants::PropertyIndexMax ?
                AdjustValidSlotIndexForInlineSlots(static_cast<PropertyIndex>(propertyIndex)) : Constants::NoSlot;
            info.isAuxSlot = propertyIndex >= GetInlineSlotCapacity();
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

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex)
    {
        uint propertyCount = record.propertyCount;
        EquivalentPropertyEntry* properties = record.properties;
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

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry *entry)
    {
        return this->IsObjTypeSpecEquivalentImpl<true>(type, entry);
    }

    template <typename T>
    template <bool doLock>
    bool DictionaryTypeHandlerBase<T>::IsObjTypeSpecEquivalentImpl(const Type* type, const EquivalentPropertyEntry *entry)
    {
        ScriptContext* scriptContext = type->GetScriptContext();

        T absSlotIndex = Constants::NoSlot;
        PropertyIndex relSlotIndex = Constants::NoSlot;

        const PropertyRecord* propertyRecord =
            doLock ? scriptContext->GetPropertyNameLocked(entry->propertyId) : scriptContext->GetPropertyName(entry->propertyId);
        DictionaryPropertyDescriptor<T>* descriptor;
        if (this->propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {
            // We don't object type specialize accessors at this point, so if we see an accessor on an object we must have a mismatch.
            // When we add support for accessors we will need another bit on EquivalentPropertyEntry indicating whether we expect
            // a data or accessor property.
            if (descriptor->IsAccessor)
            {
                return false;
            }

            absSlotIndex = descriptor->GetDataPropertyIndex<false>();
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

            if (entry->mustBeWritable && (!(descriptor->Attributes & PropertyWritable) || !descriptor->IsInitialized || descriptor->IsFixed))
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

    template <typename T>
    template <bool allowLetConstGlobal>
    PropertyIndex DictionaryTypeHandlerBase<T>::GetPropertyIndex_Internal(PropertyRecord const* propertyRecord)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        if (propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {
            return descriptor->GetDataPropertyIndex<allowLetConstGlobal>();
        }
        else
        {
            return NoSlots;
        }
    }

    template <>
    template <bool allowLetConstGlobal>
    PropertyIndex DictionaryTypeHandlerBase<BigPropertyIndex>::GetPropertyIndex_Internal(PropertyRecord const* propertyRecord)
    {
        DictionaryPropertyDescriptor<BigPropertyIndex>* descriptor;
        if (propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {
            BigPropertyIndex dataPropertyIndex = descriptor->GetDataPropertyIndex<allowLetConstGlobal>();
            if(dataPropertyIndex < Constants::NoSlot)
            {
                return (PropertyIndex)dataPropertyIndex;
            }
        }
        return Constants::NoSlot;
    }

    template <>
    PropertyIndex DictionaryTypeHandlerBase<BigPropertyIndex>::GetRootPropertyIndex(PropertyRecord const* propertyRecord)
    {
        return Constants::NoSlot;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::Add(
        const PropertyRecord* propertyId,
        PropertyAttributes attributes,
        ScriptContext *const scriptContext)
    {
        return Add(propertyId, attributes, true, false, false, scriptContext);
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::Add(

        const PropertyRecord* propertyId,
        PropertyAttributes attributes,
        bool isInitialized, bool isFixed, bool usedAsFixed,
        ScriptContext *const scriptContext)
    {
        Assert(this->GetSlotCapacity() <= MaxPropertyIndexSize);   // slotCapacity should never exceed MaxPropertyIndexSize
        Assert(nextPropertyIndex < this->GetSlotCapacity());       // nextPropertyIndex must be ready
        T index = nextPropertyIndex++;

        DictionaryPropertyDescriptor<T> descriptor(index, attributes);
        Assert((!isFixed && !usedAsFixed) || (!IsInternalPropertyId(propertyId->GetPropertyId()) && this->singletonInstance != nullptr));
        descriptor.IsInitialized = isInitialized;
        descriptor.IsFixed = isFixed;
        descriptor.UsedAsFixed = usedAsFixed;
        propertyMap->Add(propertyId, descriptor);

        if (!(attributes & PropertyWritable))
        {
            this->ClearHasOnlyWritableDataProperties();
            if(GetFlags() & IsPrototypeFlag)
            {
                scriptContext->InvalidateStoreFieldCaches(propertyId->GetPropertyId());
                scriptContext->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
        }
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::HasProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl)
    {
        return HasProperty_Internal<false>(instance, propertyId, noRedecl, nullptr);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::HasRootProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl, bool *pDeclaredProperty = nullptr)
    {
        return HasProperty_Internal<true>(instance, propertyId, noRedecl, pDeclaredProperty);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    BOOL DictionaryTypeHandlerBase<T>::HasProperty_Internal(DynamicObject* instance, PropertyId propertyId, bool *noRedecl, bool *pDeclaredProperty)
    {
        // HasProperty is called with NoProperty in JavascriptDispatch.cpp to for undeferral of the
        // deferred type system that DOM objects use.  Allow NoProperty for this reason, but only
        // here in HasProperty.
        if (propertyId == Constants::NoProperty)
        {
            return false;
        }

        DictionaryPropertyDescriptor<T>* descriptor;
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
            return DictionaryTypeHandlerBase<T>::HasItem(instance, propertyRecord->GetNumericValue());
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        DictionaryPropertyDescriptor<T>* descriptor;
        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {
            if ((descriptor->Attributes & PropertyDeleted) || !descriptor->HasNonLetConstGlobal())
            {
                return false;
            }
            return true;
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetRootProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId,
        Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return GetProperty_Internal<true>(instance, originalInstance, propertyId, value, info, requestContext);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId,
        Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return GetProperty_Internal<false>(instance, originalInstance, propertyId, value, info, requestContext);
    }

    template <typename T>
    template <bool allowLetConstGlobal, typename PropertyType>
    BOOL DictionaryTypeHandlerBase<T>::GetPropertyFromDescriptor(DynamicObject* instance, Var originalInstance,
        DictionaryPropertyDescriptor<T>* descriptor, Var* value, PropertyValueInfo* info, PropertyType propertyT, ScriptContext* requestContext)
    {
        bool const isLetConstGlobal = (descriptor->Attributes & PropertyLetConstGlobal) != 0;
        AssertMsg(!isLetConstGlobal || RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
        if (allowLetConstGlobal)
        {
            // GetRootProperty: false if not global
            if (!(descriptor->Attributes & PropertyLetConstGlobal) && (descriptor->Attributes & PropertyDeleted))
            {
                return false;
            }
        }
        else
        {
            // GetProperty: don't count deleted or global.
            if (descriptor->Attributes & (PropertyDeleted | (descriptor->IsShadowed ? 0 : PropertyLetConstGlobal)))
            {
                return false;
            }
        }

        T dataSlot = descriptor->GetDataPropertyIndex<allowLetConstGlobal>();
        if (dataSlot != NoSlots)
        {
            *value = instance->GetSlot(dataSlot);
            SetPropertyValueInfo(info, instance, dataSlot, descriptor->Attributes);
            if (!descriptor->IsInitialized || descriptor->IsFixed)
            {
                PropertyValueInfo::DisableStoreFieldCache(info);
            }
            if (descriptor->Attributes & PropertyDeleted)
            {
                // letconst shadowing a deleted property. don't bother to cache
                PropertyValueInfo::SetNoCache(info, instance);
            }
        }
        else if (descriptor->GetGetterPropertyIndex() != NoSlots)
        {
            // We must update cache before calling a getter, because it can invalidate something. Bug# 593815
            SetPropertyValueInfo(info, instance, descriptor->GetGetterPropertyIndex(), descriptor->Attributes);
            CacheOperators::CachePropertyReadForGetter(info, originalInstance, propertyT, requestContext);
            PropertyValueInfo::SetNoCache(info, instance); // we already cached getter, so we don't have to do it once more

            RecyclableObject* func = RecyclableObject::FromVar(instance->GetSlot(descriptor->GetGetterPropertyIndex()));
            *value = JavascriptOperators::CallGetter(func, originalInstance, requestContext);
            return true;
        }
        else
        {
            *value = instance->GetLibrary()->GetUndefined();
            return true;
        }
        return true;
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    BOOL DictionaryTypeHandlerBase<T>::GetProperty_Internal(DynamicObject* instance, Var originalInstance, PropertyId propertyId,
        Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            return GetPropertyFromDescriptor<allowLetConstGlobal>(instance, originalInstance, descriptor, value, info, propertyId, requestContext);
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {
            return DictionaryTypeHandlerBase<T>::GetItem(instance, originalInstance, propertyRecord->GetNumericValue(), value, requestContext);
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString,
        Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        DictionaryPropertyDescriptor<T>* descriptor;
        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {
            return GetPropertyFromDescriptor<false>(instance, originalInstance, descriptor, value, info, propertyName, requestContext);
        }

        return false;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::SetPropertyValueInfo(PropertyValueInfo* info, RecyclableObject* instance, T propIndex, PropertyAttributes attributes, InlineCacheFlags flags)
    {
        PropertyValueInfo::Set(info, instance, propIndex, attributes, flags);
    }


    template <>
    void DictionaryTypeHandlerBase<BigPropertyIndex>::SetPropertyValueInfo(PropertyValueInfo* info, RecyclableObject* instance, BigPropertyIndex propIndex, PropertyAttributes attributes, InlineCacheFlags flags)
    {
        PropertyValueInfo::SetNoCache(info, instance);
    }

    template <typename T>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return GetSetter_Internal<false>(instance, propertyId, setterValue, info, requestContext);
    }

    template <typename T>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetRootSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return GetSetter_Internal<true>(instance, propertyId, setterValue, info, requestContext);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetSetter_Internal(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        DictionaryPropertyDescriptor<T>* descriptor;

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            return GetSetterFromDescriptor<allowLetConstGlobal>(instance, descriptor, setterValue, info);
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {
            return DictionaryTypeHandlerBase<T>::GetItemSetter(instance, propertyRecord->GetNumericValue(), setterValue, requestContext);
        }

        return None;
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetSetterFromDescriptor(DynamicObject* instance, DictionaryPropertyDescriptor<T> * descriptor, Var* setterValue, PropertyValueInfo* info)
    {
        if (descriptor->Attributes & PropertyDeleted)
        {
            return None;
        }
        if (descriptor->GetDataPropertyIndex<allowLetConstGlobal>() != NoSlots)
        {
            // not a setter but shadows
            if (allowLetConstGlobal && (descriptor->Attributes & PropertyLetConstGlobal))
            {
                return (descriptor->Attributes & PropertyConst) ? (DescriptorFlags)(Const | Data) : WritableData;
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
        else if (descriptor->GetSetterPropertyIndex() != NoSlots)
        {
            *setterValue=((DynamicObject*)instance)->GetSlot(descriptor->GetSetterPropertyIndex());
            SetPropertyValueInfo(info, instance, descriptor->GetSetterPropertyIndex(), descriptor->Attributes, InlineCacheSetterFlag);
            return Accessor;
        }
        return None;
    }

    template <typename T>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        DictionaryPropertyDescriptor<T>* descriptor;

        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {
            return GetSetterFromDescriptor<false>(instance, descriptor, setterValue, info);
        }

        return None;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetRootProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return SetProperty_Internal<true>(instance, propertyId, value, flags, info);
    }
    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::InitProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        return SetProperty_Internal<false>(instance, propertyId, value, flags, info, true /* IsInit */);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        return SetProperty_Internal<false>(instance, propertyId, value, flags, info);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    void DictionaryTypeHandlerBase<T>::SetPropertyWithDescriptor(DynamicObject* instance, PropertyId propertyId, DictionaryPropertyDescriptor<T> * descriptor,
        Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        Assert(instance);
        Assert((descriptor->Attributes & PropertyDeleted) == 0 || (allowLetConstGlobal && descriptor->IsShadowed));

        // DictionaryTypeHandlers are not supposed to be shared.
        Assert(!GetIsOrMayBecomeShared());
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        Assert(this->singletonInstance == nullptr || localSingletonInstance == instance);
        T dataSlot = descriptor->GetDataPropertyIndex<allowLetConstGlobal>();
        if (dataSlot != NoSlots)
        {
            if (allowLetConstGlobal
                && (descriptor->Attributes & PropertyNoRedecl)
                && !(flags & PropertyOperation_AllowUndecl))
            {
                ScriptContext* scriptContext = instance->GetScriptContext();
                if (scriptContext->IsUndeclBlockVar(instance->GetSlot(dataSlot)))
                {
                    JavascriptError::ThrowReferenceError(scriptContext, JSERR_UseBeforeDeclaration);
                }
            }

            if (!descriptor->IsInitialized)
            {
                if ((flags & PropertyOperation_PreInit) == 0)
                {
                    descriptor->IsInitialized = true;
                    if (localSingletonInstance == instance && !IsInternalPropertyId(propertyId) &&
                        (flags & (PropertyOperation_NonFixedValue | PropertyOperation_SpecialValue)) == 0)
                    {
                        Assert(value != nullptr);
                        // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
                        Assert(!instance->IsExternal());
                        descriptor->IsFixed = (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : (ShouldFixDataProperties() && CheckHeuristicsForFixedDataProps(instance, propertyId, value)));
                    }
                }
            }
            else
            {
                InvalidateFixedField(instance, propertyId, descriptor);
            }

            SetSlotUnchecked(instance, dataSlot, value);

            // If we just added a fixed method, don't populate the inline cache so that we always take the slow path
            // when overwriting this property and correctly invalidate any JIT-ed code that hard-coded this method.
            if (descriptor->IsInitialized && !descriptor->IsFixed)
            {
                SetPropertyValueInfo(info, instance, dataSlot, GetLetConstGlobalPropertyAttributes<allowLetConstGlobal>(descriptor->Attributes));
            }
            else
            {
                PropertyValueInfo::SetNoCache(info, instance);
            }
        }
        else if (descriptor->GetSetterPropertyIndex() != NoSlots)
        {
            RecyclableObject* func = RecyclableObject::FromVar(instance->GetSlot(descriptor->GetSetterPropertyIndex()));
            JavascriptOperators::CallSetter(func, instance, value, NULL);

            // Wait for the setter to return before setting up the inline cache info, as the setter may change
            // the attributes
            T dataSlot = descriptor->GetDataPropertyIndex<false>();
            if (dataSlot != NoSlots)
            {
                SetPropertyValueInfo(info, instance, dataSlot, descriptor->Attributes);
            }
            else if (descriptor->GetSetterPropertyIndex() != NoSlots)
            {
                SetPropertyValueInfo(info, instance, descriptor->GetSetterPropertyIndex(), descriptor->Attributes, InlineCacheSetterFlag);
            }
        }
        SetPropertyUpdateSideEffect(instance, propertyId, value, SideEffects_Any);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    BOOL DictionaryTypeHandlerBase<T>::SetProperty_Internal(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info, bool isInit)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T>* descriptor;
        bool throwIfNotExtensible = (flags & (PropertyOperation_ThrowIfNotExtensible | PropertyOperation_StrictMode)) != 0;
        bool isForce = (flags & PropertyOperation_Force) != 0;

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            Assert(descriptor->SanityCheckFixedBits());
            if (descriptor->Attributes & PropertyDeleted)
            {
                if (!isForce)
                {
                    if (!this->VerifyIsExtensible(scriptContext, throwIfNotExtensible))
                    {
                        return false;
                    }
                }
                scriptContext->InvalidateProtoCaches(propertyId);
                if (descriptor->Attributes & PropertyLetConstGlobal)
                {
                    descriptor->Attributes = PropertyDynamicTypeDefaults | (descriptor->Attributes & (PropertyLetConstGlobal | PropertyNoRedecl));
                }
                else
                {
                    descriptor->Attributes = PropertyDynamicTypeDefaults;
                }
                instance->SetHasNoEnumerableProperties(false);
                descriptor->ConvertToData();
            }
            else if (!allowLetConstGlobal && descriptor->HasNonLetConstGlobal() && !(descriptor->Attributes & PropertyWritable))
            {
                if (!isForce)
                {
                    JavascriptError::ThrowCantAssignIfStrictMode(flags, scriptContext);
                }

                // Since we separate LdFld and StFld caches there is no point in caching for StFld with non-writable properties, except perhaps
                // to prepopulate the type property cache (which we do share between LdFld and StFld), for potential future field loads.  This
                // would require additional handling in CacheOperators::CachePropertyWrite, such that for !info-IsWritable() we don't populate
                // the local cache (that would be illegal), but still populate the type's property cache.
                PropertyValueInfo::SetNoCache(info, instance);
                return false;
            }
            else if (isInit && descriptor->IsAccessor)
            {
                descriptor->ConvertToData();
            }
            SetPropertyWithDescriptor<allowLetConstGlobal>(instance, propertyId, descriptor, value, flags, info);
            return true;
        }

        // Always check numeric propertyRecord. This may create objectArray.
        if (propertyRecord->IsNumeric())
        {
            // Calls this or subclass implementation
            return SetItem(instance, propertyRecord->GetNumericValue(), value, flags);
        }
        return this->AddProperty(instance, propertyRecord, value, PropertyDynamicTypeDefaults, info, flags, throwIfNotExtensible, SideEffects_Any);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        // Either the property exists in the dictionary, in which case a PropertyRecord for it exists,
        // or we have to add it to the dictionary, in which case we need to get or create a PropertyRecord.
        // Thus, just get or create one and call the PropertyId overload of SetProperty.
        PropertyRecord const * propertyRecord;
        instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return DictionaryTypeHandlerBase<T>::SetProperty(instance, propertyRecord->GetPropertyId(), value, flags, info);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {
        return DeleteProperty_Internal<false>(instance, propertyId, propertyOperationFlags);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::DeleteRootProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return DeleteProperty_Internal<true>(instance, propertyId, propertyOperationFlags);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    BOOL DictionaryTypeHandlerBase<T>::DeleteProperty_Internal(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            Assert(descriptor->SanityCheckFixedBits());

            if (descriptor->Attributes & PropertyDeleted)
            {
                // If PropertyDeleted and PropertyLetConstGlobal are set then we have both
                // a deleted global property and let/const variable in this descriptor.
                // If allowLetConstGlobal is true then the let/const shadows the property
                // and we should return false for a failed delete by going into the else
                // if branch below.
                if (allowLetConstGlobal && (descriptor->Attributes & PropertyLetConstGlobal))
                {
                    JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, scriptContext, propertyRecord->GetBuffer());

                    return false;
                }
                return true;
            }
            else if (!(descriptor->Attributes & PropertyConfigurable) ||
                (allowLetConstGlobal && (descriptor->Attributes & PropertyLetConstGlobal)))
            {
                // Let/const properties do not have attributes and they cannot be deleted
                JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, scriptContext, scriptContext->GetPropertyName(propertyId)->GetBuffer());

                return false;
            }

            Var undefined = scriptContext->GetLibrary()->GetUndefined();

            if (descriptor->HasNonLetConstGlobal())
            {
                T dataSlot = descriptor->GetDataPropertyIndex<false>();
                if (dataSlot != NoSlots)
                {
                    SetSlotUnchecked(instance, dataSlot, undefined);
                }
                else
                {
                    Assert(descriptor->IsAccessor);
                    SetSlotUnchecked(instance, descriptor->GetGetterPropertyIndex(), undefined);
                    SetSlotUnchecked(instance, descriptor->GetSetterPropertyIndex(), undefined);
                }

                if (this->GetFlags() & IsPrototypeFlag)
                {
                    scriptContext->InvalidateProtoCaches(propertyId);
                }

                if ((descriptor->Attributes & PropertyLetConstGlobal) == 0)
                {
                    Assert(!descriptor->IsShadowed);
                    descriptor->Attributes = PropertyDeletedDefaults;
                }
                else
                {
                    descriptor->Attributes &= ~PropertyDynamicTypeDefaults;
                    descriptor->Attributes |= PropertyDeletedDefaults;
                }
                InvalidateFixedField(instance, propertyId, descriptor);

                // Change the type so as we can invalidate the cache in fast path jit
                instance->ChangeType();
                SetPropertyUpdateSideEffect(instance, propertyId, nullptr, SideEffects_Any);
                return true;
            }

            Assert(descriptor->Attributes & PropertyLetConstGlobal);
            return false;
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {
            return DictionaryTypeHandlerBase<T>::DeleteItem(instance, propertyRecord->GetNumericValue(), propertyOperationFlags);
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsFixedProperty(const DynamicObject* instance, PropertyId propertyId)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T> descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetValue(propertyRecord, &descriptor))
        {
            return descriptor.IsFixed;
        }
        else
        {
            AssertMsg(false, "Asking about a property this type handler doesn't know about?");
            return false;
        }
    }

        template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetItem(DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags)
    {
        if (!(this->GetFlags() & IsExtensibleFlag) && !instance->HasObjectArray())
        {
            ScriptContext* scriptContext = instance->GetScriptContext();
            JavascriptError::ThrowCantExtendIfStrictMode(flags, scriptContext);
            return false;
        }
        return __super::SetItem(instance, index, value, flags);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetItemWithAttributes(DynamicObject* instance, uint32 index, Var value, PropertyAttributes attributes)
    {
        return instance->SetObjectArrayItemWithAttributes(index, value, attributes);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetItemAttributes(DynamicObject* instance, uint32 index, PropertyAttributes attributes)
    {
        return instance->SetObjectArrayItemAttributes(index, attributes);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetItemAccessors(DynamicObject* instance, uint32 index, Var getter, Var setter)
    {
        return instance->SetObjectArrayItemAccessors(index, getter, setter);
    }

    template <typename T>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetItemSetter(DynamicObject* instance, uint32 index, Var* setterValue, ScriptContext* requestContext)
    {
        if (instance->HasObjectArray())
        {
            return instance->GetObjectArrayItemSetter(index, setterValue, requestContext);
        }
        return __super::GetItemSetter(instance, index, setterValue, requestContext);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if (!descriptor->HasNonLetConstGlobal())
            {
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");

                return true;
            }
            return descriptor->Attributes & PropertyEnumerable;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {
                return objectArray->IsEnumerable(propertyId);
            }
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if (!descriptor->HasNonLetConstGlobal())
            {
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return !(descriptor->Attributes & PropertyConst);
            }
            return descriptor->Attributes & PropertyWritable;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {
                return objectArray->IsWritable(propertyId);
            }
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if (!descriptor->HasNonLetConstGlobal())
            {
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return true;
            }
            return descriptor->Attributes & PropertyConfigurable;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {
                return objectArray->IsConfigurable(propertyId);
            }
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                return false;
            }

            if (!descriptor->HasNonLetConstGlobal())
            {
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return false;
            }

            if (value)
            {
                descriptor->Attributes |= PropertyEnumerable;
                instance->SetHasNoEnumerableProperties(false);
            }
            else
            {
                descriptor->Attributes &= (~PropertyEnumerable);
            }
            return true;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {
                return objectArray->SetEnumerable(propertyId, value);
            }
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        ScriptContext* scriptContext = instance->GetScriptContext();
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                return false;
            }

            if (!descriptor->HasNonLetConstGlobal())
            {
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return false;
            }

            if (value)
            {
                descriptor->Attributes |= PropertyWritable;
            }
            else
            {
                descriptor->Attributes &= (~PropertyWritable);
                this->ClearHasOnlyWritableDataProperties();
                if(GetFlags() & IsPrototypeFlag)
                {
                    scriptContext->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }
            instance->ChangeType();
            return true;
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {
            return instance->SetObjectArrayItemWritable(propertyId, value);
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                return false;
            }

            if (!descriptor->HasNonLetConstGlobal())
            {
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return false;
            }

            if (value)
            {
                descriptor->Attributes |= PropertyConfigurable;
            }
            else
            {
                descriptor->Attributes &= (~PropertyConfigurable);
            }
            return true;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {
                return objectArray->SetConfigurable(propertyId, value);
            }
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::PreventExtensions(DynamicObject* instance)
    {
        this->ClearFlags(IsExtensibleFlag);

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray)
        {
            objectArray->PreventExtensions();
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::Seal(DynamicObject* instance)
    {
        this->ClearFlags(IsExtensibleFlag);

        // Set [[Configurable]] flag of each property to false
        DictionaryPropertyDescriptor<T> *descriptor = nullptr;
        for (T index = 0; index < propertyMap->Count(); index++)
        {
            descriptor = propertyMap->GetReferenceAt(index);
            if (descriptor->HasNonLetConstGlobal())
            {
                descriptor->Attributes &= (~PropertyConfigurable);
            }
        }

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray)
        {
            objectArray->Seal();
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {
        this->ClearFlags(IsExtensibleFlag);

        // Set [[Writable]] flag of each property to false except for setter\getters
        // Set [[Configurable]] flag of each property to false
        DictionaryPropertyDescriptor<T> *descriptor = nullptr;
        for (T index = 0; index < propertyMap->Count(); index++)
        {
            descriptor = propertyMap->GetReferenceAt(index);
            if (descriptor->HasNonLetConstGlobal())
            {
                if (descriptor->GetDataPropertyIndex<false>() != NoSlots)
                {
                    // Only data descriptor has Writable property
                    descriptor->Attributes &= ~(PropertyWritable | PropertyConfigurable);
                }
                else
                {
                    descriptor->Attributes &= ~(PropertyConfigurable);
                }
            }
#if DBG
            else
            {
                AssertMsg(RootObjectBase::Is(instance), "instance needs to be global object when letconst global is set");
            }
#endif
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
        if(GetFlags() & IsPrototypeFlag)
        {
            InvalidateStoreFieldCachesForAllProperties(instance->GetScriptContext());
            instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsSealed(DynamicObject* instance)
    {
        if (this->GetFlags() & IsExtensibleFlag)
        {
            return false;
        }

        DictionaryPropertyDescriptor<T> *descriptor = nullptr;
        for (T index = 0; index < propertyMap->Count(); index++)
        {
            descriptor = propertyMap->GetReferenceAt(index);
            if ((!(descriptor->Attributes & PropertyDeleted) && !(descriptor->Attributes & PropertyLetConstGlobal)))
            {
                if (descriptor->Attributes & PropertyConfigurable)
                {
                    // [[Configurable]] must be false for all (existing) properties.
                    // IE9 compatibility: keep IE9 behavior (also check deleted properties)
                    return false;
                }
            }
        }

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray && !objectArray->IsSealed())
        {
            return false;
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsFrozen(DynamicObject* instance)
    {
        if (this->GetFlags() & IsExtensibleFlag)
        {
            return false;
        }

        DictionaryPropertyDescriptor<T> *descriptor = nullptr;
        for (T index = 0; index < propertyMap->Count(); index++)
        {
            descriptor = propertyMap->GetReferenceAt(index);
            if ((!(descriptor->Attributes & PropertyDeleted) && !(descriptor->Attributes & PropertyLetConstGlobal)))
            {
                if (descriptor->Attributes & PropertyConfigurable)
                {
                    return false;
                }

                if (descriptor->GetDataPropertyIndex<false>() != NoSlots && (descriptor->Attributes & PropertyWritable))
                {
                    // Only data descriptor has [[Writable]] property
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

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetAccessors(DynamicObject* instance, PropertyId propertyId, Var* getter, Var* setter)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        ScriptContext* scriptContext = instance->GetScriptContext();
        AssertMsg(nullptr != getter && nullptr != setter, "Getter/Setter must be a valid pointer" );

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                return false;
            }

            if (descriptor->GetDataPropertyIndex<false>() == NoSlots)
            {
                bool getset = false;
                if (descriptor->GetGetterPropertyIndex() != NoSlots)
                {
                    *getter = instance->GetSlot(descriptor->GetGetterPropertyIndex());
                    getset = true;
                }
                if (descriptor->GetSetterPropertyIndex() != NoSlots)
                {
                    *setter = instance->GetSlot(descriptor->GetSetterPropertyIndex());
                    getset = true;
                }
                return getset;
            }
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {
                return objectArray->GetAccessors(propertyId, getter, setter, scriptContext);
            }
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {
        Assert(instance);
        JavascriptLibrary* library = instance->GetLibrary();
        ScriptContext* scriptContext = instance->GetScriptContext();

        Assert(this->VerifyIsExtensible(scriptContext, false) || this->HasProperty(instance, propertyId)
            || JavascriptFunction::IsBuiltinProperty(instance, propertyId));

        DictionaryPropertyDescriptor<T>* descriptor;
        if (this->GetFlags() & IsPrototypeFlag)
        {
            scriptContext->InvalidateProtoCaches(propertyId);
        }

        bool isGetterSet = true;
        bool isSetterSet = true;
        if (!getter || getter == library->GetDefaultAccessorFunction())
        {
            isGetterSet = false;
        }
        if (!setter || setter == library->GetDefaultAccessorFunction())
        {
            isSetterSet = false;
        }

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            Assert(descriptor->SanityCheckFixedBits());

            if (descriptor->Attributes & PropertyDeleted)
            {
                if (descriptor->Attributes & PropertyLetConstGlobal)
                {
                    descriptor->Attributes = PropertyDynamicTypeDefaults | (descriptor->Attributes & (PropertyLetConstGlobal | PropertyNoRedecl));
                }
                else
                {
                    descriptor->Attributes = PropertyDynamicTypeDefaults;
                }
            }

            if (!descriptor->IsAccessor)
            {
                // New getter/setter, make sure both values are not null and set to the slots
                getter = CanonicalizeAccessor(getter, library);
                setter = CanonicalizeAccessor(setter, library);
            }

            // conversion from data-property to accessor property
            if (descriptor->ConvertToGetterSetter(nextPropertyIndex))
            {
                if (this->GetSlotCapacity() <= nextPropertyIndex)
                {
                    if (this->GetSlotCapacity() >= MaxPropertyIndexSize)
                    {
                        Throw::OutOfMemory();
                    }

                    this->EnsureSlotCapacity(instance);
                }
            }

            // DictionaryTypeHandlers are not supposed to be shared.
            Assert(!GetIsOrMayBecomeShared());
            DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
            Assert(this->singletonInstance == nullptr || localSingletonInstance == instance);

            // Although we don't actually have CreateTypeForNewScObject on DictionaryTypeHandler, we could potentially
            // transition to a DictionaryTypeHandler with some properties uninitialized.
            if (!descriptor->IsInitialized)
            {
                descriptor->IsInitialized = true;
                if (localSingletonInstance == instance && !IsInternalPropertyId(propertyId))
                {
                    // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
                    Assert(!instance->IsExternal() || (flags & PropertyOperation_NonFixedValue) != 0);
                    descriptor->IsFixed = (flags & PropertyOperation_NonFixedValue) == 0 && ShouldFixAccessorProperties();
                }
                if (!isGetterSet || !isSetterSet)
                {
                    descriptor->IsOnlyOneAccessorInitialized = true;
                }
            }
            else if (descriptor->IsOnlyOneAccessorInitialized)
            {
                // Only one of getter/setter was initialized, allow the isFixed to stay if we are defining the other one.
                Var oldGetter = GetSlot(instance, descriptor->GetGetterPropertyIndex());
                Var oldSetter = GetSlot(instance, descriptor->GetSetterPropertyIndex());

                if (((getter == oldGetter || !isGetterSet) && oldSetter == library->GetDefaultAccessorFunction()) ||
                    ((setter == oldSetter || !isSetterSet) && oldGetter == library->GetDefaultAccessorFunction()))
                {
                    descriptor->IsOnlyOneAccessorInitialized = false;
                }
                else
                {
                    InvalidateFixedField(instance, propertyId, descriptor);
                }
            }
            else
            {
                InvalidateFixedField(instance, propertyId, descriptor);
            }

            // don't overwrite an existing accessor with null
            if (getter != nullptr)
            {
                getter = CanonicalizeAccessor(getter, library);
                SetSlotUnchecked(instance, descriptor->GetGetterPropertyIndex(), getter);
            }
            if (setter != nullptr)
            {
                setter = CanonicalizeAccessor(setter, library);
                SetSlotUnchecked(instance, descriptor->GetSetterPropertyIndex(), setter);
            }
            instance->ChangeType();
            this->ClearHasOnlyWritableDataProperties();
            if(GetFlags() & IsPrototypeFlag)
            {
                scriptContext->InvalidateStoreFieldCaches(propertyId);
                library->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
            SetPropertyUpdateSideEffect(instance, propertyId, nullptr, SideEffects_Any);

            // Let's make sure we always have a getter and a setter
            Assert(instance->GetSlot(descriptor->GetGetterPropertyIndex()) != nullptr && instance->GetSlot(descriptor->GetSetterPropertyIndex()) != nullptr);

            return true;
        }

        // Always check numeric propertyRecord. This may create objectArray.
        if (propertyRecord->IsNumeric())
        {
            // Calls this or subclass implementation
            return SetItemAccessors(instance, propertyRecord->GetNumericValue(), getter, setter);
        }

        getter = CanonicalizeAccessor(getter, library);
        setter = CanonicalizeAccessor(setter, library);
        T getterIndex = nextPropertyIndex++;
        T setterIndex = nextPropertyIndex++;
        DictionaryPropertyDescriptor<T> newDescriptor(getterIndex, setterIndex);
        if (this->GetSlotCapacity() <= nextPropertyIndex)
        {
            if (this->GetSlotCapacity() >= MaxPropertyIndexSize)
            {
                Throw::OutOfMemory();
            }

            this->EnsureSlotCapacity(instance);
        }

        // DictionaryTypeHandlers are not supposed to be shared.
        Assert(!GetIsOrMayBecomeShared());
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        Assert(this->singletonInstance == nullptr || localSingletonInstance == instance);
        newDescriptor.IsInitialized = true;
        if (localSingletonInstance == instance && !IsInternalPropertyId(propertyId))
        {
            // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
            Assert(!instance->IsExternal() || (flags & PropertyOperation_NonFixedValue) != 0);

            // Even if one (or both?) accessors are the default functions obtained through cannonicalization,
            // they are still legitimate functions, so it's ok to mark the whole property as fixed.
            newDescriptor.IsFixed = (flags & PropertyOperation_NonFixedValue) == 0 && ShouldFixAccessorProperties();
            if (!isGetterSet || !isSetterSet)
            {
                newDescriptor.IsOnlyOneAccessorInitialized = true;
            }
        }

        propertyMap->Add(propertyRecord, newDescriptor);

        SetSlotUnchecked(instance, newDescriptor.GetGetterPropertyIndex(), getter);
        SetSlotUnchecked(instance, newDescriptor.GetSetterPropertyIndex(), setter);
        this->ClearHasOnlyWritableDataProperties();
        if(GetFlags() & IsPrototypeFlag)
        {
            scriptContext->InvalidateStoreFieldCaches(propertyId);
            library->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }
        SetPropertyUpdateSideEffect(instance, propertyId, nullptr, SideEffects_Any);

        // Let's make sure we always have a getter and a setter
        Assert(instance->GetSlot(newDescriptor.GetGetterPropertyIndex()) != nullptr && instance->GetSlot(newDescriptor.GetSetterPropertyIndex()) != nullptr);

        return true;
    }

    // If this type is not extensible and the property being set does not already exist,
    // if throwIfNotExtensible is
    // * true, a type error will be thrown
    // * false, FALSE will be returned (unless strict mode is enabled, in which case a type error will be thrown).
    // Either way, the property will not be set.
    //
    // This is used to ensure that we throw in the following scenario, in accordance with
    // section 10.2.1.2.2 of the Errata to the ES5 spec:
    //    Object.preventExtension(this);  // make the global object non-extensible
    //    var x = 4;
    //
    // throwIfNotExtensible should always be false for non-numeric properties.
    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value,
        PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        ScriptContext* scriptContext = instance->GetScriptContext();
        bool isForce = (flags & PropertyOperation_Force) != 0;
        bool throwIfNotExtensible = (flags & PropertyOperation_ThrowIfNotExtensible) != 0;

#ifdef DEBUG
        uint32 debugIndex;
        Assert(!(throwIfNotExtensible && scriptContext->IsNumericPropertyId(propertyId, &debugIndex)));
#endif
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            Assert(descriptor->SanityCheckFixedBits());

            if (attributes & descriptor->Attributes & PropertyLetConstGlobal)
            {
                // Do not need to change the descriptor or its attributes if setting the initial value of a LetConstGlobal
            }
            else if (descriptor->Attributes & PropertyDeleted && !(attributes & PropertyLetConstGlobal))
            {
                if (!isForce)
                {
                    if (!this->VerifyIsExtensible(scriptContext, throwIfNotExtensible))
                    {
                        return FALSE;
                    }
                }

                scriptContext->InvalidateProtoCaches(propertyId);
                if (descriptor->Attributes & PropertyLetConstGlobal)
                {
                    descriptor->Attributes = attributes | (descriptor->Attributes & (PropertyLetConstGlobal | PropertyNoRedecl));
                }
                else
                {
                    descriptor->Attributes = attributes;
                }
                descriptor->ConvertToData();
            }
            else if (descriptor->IsShadowed)
            {
                descriptor->Attributes = attributes | (descriptor->Attributes & (PropertyLetConstGlobal | PropertyNoRedecl));
            }
            else if ((descriptor->Attributes & PropertyLetConstGlobal) != (attributes & PropertyLetConstGlobal))
            {
                bool addingLetConstGlobal = (attributes & PropertyLetConstGlobal) != 0;

                descriptor->AddShadowedData(nextPropertyIndex, addingLetConstGlobal);

                if (addingLetConstGlobal)
                {
                    descriptor->Attributes = descriptor->Attributes | (attributes & PropertyNoRedecl) | PropertyLetConstGlobal;
                }
                else
                {
                    descriptor->Attributes = attributes | (descriptor->Attributes & PropertyNoRedecl) | PropertyLetConstGlobal;
                }

                if (this->GetSlotCapacity() <= nextPropertyIndex)
                {
                    if (this->GetSlotCapacity() >= MaxPropertyIndexSize)
                    {
                        Throw::OutOfMemory();
                    }

                    this->EnsureSlotCapacity(instance);
                }

                if (addingLetConstGlobal)
                {
                    // If shadowing a global property with a let/const, need to invalidate
                    // JIT fast path cache since look up could now go to the let/const instead
                    // of the global property.
                    //
                    // Do not need to invalidate when adding a global property that gets shadowed
                    // by an existing let/const, since all caches will still be correct.
                    instance->ChangeType();
                }
            }
            else
            {
                if (descriptor->IsAccessor && !(attributes & PropertyLetConstGlobal))
                {
                    AssertMsg(RootObjectBase::Is(instance) || JavascriptFunction::IsBuiltinProperty(instance, propertyId) ||
                        // ValidateAndApplyPropertyDescriptor says to preserve Configurable and Enumerable flags

                        // For InitRootFld, which is equivalent to
                        // CreateGlobalFunctionBinding called from GlobalDeclarationInstantiation in the spec,
                        // we can assume that the attributes specified include enumerable and writable.  Thus
                        // we don't need to preserve the original values of these two attributes and therefore
                        // do not need to change InitRootFld from being a SetPropertyWithAttributes API call to
                        // something else.  All we need to do is convert the descriptor to a data descriptor.
                        // Built-in Function.prototype properties 'length', 'arguments', and 'caller' are special cases.

                        (JavascriptOperators::IsClassConstructor(JavascriptOperators::GetProperty(instance, PropertyIds::constructor, scriptContext)) &&
                            (attributes & PropertyClassMemberDefaults) == PropertyClassMemberDefaults),
                        // 14.3.9: InitClassMember sets property descriptor to {writable:true, enumerable:false, configurable:true}

                        "Expect to only come down this path for InitClassMember or InitRootFld (on the global object) overwriting existing accessor property");
                    if (!(descriptor->Attributes & PropertyConfigurable))
                    {
                        if (scriptContext && scriptContext->GetThreadContext()->RecordImplicitException())
                        {
                            JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable, scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
                        }
                        return FALSE;
                    }
                    descriptor->ConvertToData();
                    instance->ChangeType();
                }

                // Make sure to keep the PropertyLetConstGlobal bit as is while taking the new attributes.
                descriptor->Attributes = attributes | (descriptor->Attributes & PropertyLetConstGlobal);
            }

            if (attributes & PropertyLetConstGlobal)
            {
                SetPropertyWithDescriptor<true>(instance, propertyId, descriptor, value, flags, info);
            }
            else
            {
                SetPropertyWithDescriptor<false>(instance, propertyId, descriptor, value, flags, info);
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
                    scriptContext->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }

            SetPropertyUpdateSideEffect(instance, propertyId, value, possibleSideEffects);
            return true;
        }

        // Always check numeric propertyRecord. This may create objectArray.
        if (propertyRecord->IsNumeric())
        {
            // Calls this or subclass implementation
            return SetItemWithAttributes(instance, propertyRecord->GetNumericValue(), value, attributes);
        }

        return this->AddProperty(instance, propertyRecord, value, attributes, info, flags, throwIfNotExtensible, possibleSideEffects);
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::EnsureSlotCapacity(DynamicObject * instance)
    {
        Assert(this->GetSlotCapacity() < MaxPropertyIndexSize); // Otherwise we can't grow this handler's capacity. We should've evolved to Bigger handler or OOM.

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

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        ScriptContext* scriptContext = instance->GetScriptContext();
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                return false;
            }

            descriptor->Attributes = (descriptor->Attributes & ~PropertyDynamicTypeDefaults) | (attributes & PropertyDynamicTypeDefaults);

            if (descriptor->Attributes & PropertyEnumerable)
            {
                instance->SetHasNoEnumerableProperties(false);
            }

            if (!(descriptor->Attributes & PropertyWritable))
            {
                this->ClearHasOnlyWritableDataProperties();
                if(GetFlags() & IsPrototypeFlag)
                {
                    scriptContext->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }

            return true;
        }

        // Check numeric propertyId only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {
            return DictionaryTypeHandlerBase<T>::SetItemAttributes(instance, propertyRecord->GetNumericValue(), attributes);
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {
        // this might get value that are deleted from the dictionary, but that should be nulled out
        DictionaryPropertyDescriptor<T> * descriptor;
        // We can't look it up using the slot index, as one propertyId might have multiple slots,  do the propertyId map lookup
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (!propertyMap->TryGetReference(propertyRecord, &descriptor))
        {
            return false;
        }
        // This function is only used by LdRootFld, so the index will allow let const globals
        Assert(descriptor->GetDataPropertyIndex<true>() == index);
        if (descriptor->Attributes & PropertyDeleted)
        {
            return false;
        }
        *attributes = descriptor->Attributes & PropertyDynamicTypeDefaults;
        return true;
    }

    template <typename T>
    Var DictionaryTypeHandlerBase<T>::CanonicalizeAccessor(Var accessor, /*const*/ JavascriptLibrary* library)
    {
        if (accessor == nullptr || JavascriptOperators::IsUndefinedObject(accessor, library))
        {
            accessor = library->GetDefaultAccessorFunction();
        }
        return accessor;
    }

    template <typename T>
    BigDictionaryTypeHandler* DictionaryTypeHandlerBase<T>::ConvertToBigDictionaryTypeHandler(DynamicObject* instance)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();

        BigDictionaryTypeHandler* newTypeHandler = NewBigDictionaryTypeHandler(recycler, GetSlotCapacity(), GetInlineSlotCapacity(), GetOffsetOfInlineSlots());
        // We expect the new type handler to start off marked as having only writable data properties.
        Assert(newTypeHandler->GetHasOnlyWritableDataProperties());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        DynamicType* oldType = instance->GetDynamicType();
        RecyclerWeakReference<DynamicObject>* oldSingletonInstance = GetSingletonInstance();
        TraceFixedFieldsBeforeTypeHandlerChange(L"DictionaryTypeHandler", L"BigDictionaryTypeHandler", instance, this, oldType, oldSingletonInstance);
#endif

        CopySingletonInstance(instance, newTypeHandler);

        DictionaryPropertyDescriptor<T> descriptor;
        DictionaryPropertyDescriptor<BigPropertyIndex> bigDescriptor;

        const PropertyRecord* propertyId;
        for (int i = 0; i < propertyMap->Count(); i++)
        {
            descriptor = propertyMap->GetValueAt(i);
            propertyId = propertyMap->GetKeyAt(i);

            bigDescriptor.CopyFrom(descriptor);
            newTypeHandler->propertyMap->Add(propertyId, bigDescriptor);
        }

        newTypeHandler->nextPropertyIndex = nextPropertyIndex;

        ClearSingletonInstance();

        AssertMsg((newTypeHandler->GetFlags() & IsPrototypeFlag) == 0, "Why did we create a brand new type handler with a prototype flag set?");
        newTypeHandler->SetFlags(IsPrototypeFlag, this->GetFlags());
        newTypeHandler->ChangeFlags(IsExtensibleFlag, this->GetFlags());
        // Any new type handler we expect to see here should have inline slot capacity locked.  If this were to change, we would need
        // to update our shrinking logic (see PathTypeHandlerBase::ShrinkSlotAndInlineSlotCapacity).
        Assert(newTypeHandler->GetIsInlineSlotCapacityLocked());
        newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection,  this->GetPropertyTypes());
        newTypeHandler->SetInstanceTypeHandler(instance);
        // Unlike for SimpleDictionaryTypeHandler or PathTypeHandler, the DictionaryTypeHandler copies usedAsFixed indiscriminately above.
        // Therefore, we don't care if we changed the type or not, and don't need the assert below.
        // We assumed that we don't need to transfer used as fixed bits unless we are a prototype, which is only valid if we also changed the type.
        // Assert(instance->GetType() != oldType);
        Assert(!newTypeHandler->HasSingletonInstance() || !instance->HasSharedType());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        TraceFixedFieldsAfterTypeHandlerChange(instance, this, newTypeHandler, oldType, oldSingletonInstance);
#endif

        return newTypeHandler;
    }

    template <typename T>
    BigDictionaryTypeHandler* DictionaryTypeHandlerBase<T>::NewBigDictionaryTypeHandler(Recycler* recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots)
    {
        return RecyclerNew(recycler, BigDictionaryTypeHandler, recycler, slotCapacity, inlineSlotCapacity, offsetOfInlineSlots);
    }

    template <>
    BigDictionaryTypeHandler* DictionaryTypeHandlerBase<BigPropertyIndex>::ConvertToBigDictionaryTypeHandler(DynamicObject* instance)
    {
        Throw::OutOfMemory();
    }

    template<>
    BOOL DictionaryTypeHandlerBase<PropertyIndex>::IsBigDictionaryTypeHandler()
    {
        return FALSE;
    }

    template<>
    BOOL DictionaryTypeHandlerBase<BigPropertyIndex>::IsBigDictionaryTypeHandler()
    {
        return TRUE;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::AddProperty(DynamicObject* instance, const PropertyRecord* propertyRecord, Var value,
        PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, bool throwIfNotExtensible, SideEffects possibleSideEffects)
    {
        AnalysisAssert(instance);
        ScriptContext* scriptContext = instance->GetScriptContext();
        bool isForce = (flags & PropertyOperation_Force) != 0;

#if DBG
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(!propertyMap->TryGetReference(propertyRecord, &descriptor));
        Assert(!propertyRecord->IsNumeric());
#endif

        if (!isForce)
        {
            if (!this->VerifyIsExtensible(scriptContext, throwIfNotExtensible))
            {
                return FALSE;
            }
        }

        if (this->GetSlotCapacity() <= nextPropertyIndex)
        {
            if (this->GetSlotCapacity() >= MaxPropertyIndexSize ||
                (this->GetSlotCapacity() >= CONFIG_FLAG(BigDictionaryTypeHandlerThreshold) && !this->IsBigDictionaryTypeHandler()))
            {
                BigDictionaryTypeHandler* newTypeHandler = ConvertToBigDictionaryTypeHandler(instance);

                return newTypeHandler->AddProperty(instance, propertyRecord, value, attributes, info, flags, false, possibleSideEffects);
            }
            this->EnsureSlotCapacity(instance);
        }

        T index = nextPropertyIndex++;
        DictionaryPropertyDescriptor<T> newDescriptor(index, attributes);

        // DictionaryTypeHandlers are not supposed to be shared.
        Assert(!GetIsOrMayBecomeShared());
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        Assert(this->singletonInstance == nullptr || localSingletonInstance == instance);

        if ((flags & PropertyOperation_PreInit) == 0)
        {
            newDescriptor.IsInitialized = true;
            if (localSingletonInstance == instance && !IsInternalPropertyId(propertyRecord->GetPropertyId()) &&
                (flags & (PropertyOperation_NonFixedValue | PropertyOperation_SpecialValue)) == 0)
            {
                Assert(value != nullptr);
                // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
                Assert(!instance->IsExternal());
                newDescriptor.IsFixed = (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : (ShouldFixDataProperties() & CheckHeuristicsForFixedDataProps(instance, propertyRecord, value)));
            }
        }

        propertyMap->Add(propertyRecord, newDescriptor);

        if (attributes & PropertyEnumerable)
        {
            instance->SetHasNoEnumerableProperties(false);
        }

        if (!(attributes & PropertyWritable))
        {
            this->ClearHasOnlyWritableDataProperties();
            if(GetFlags() & IsPrototypeFlag)
            {
                instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyRecord->GetPropertyId());
                instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
        }

        SetSlotUnchecked(instance, index, value);

        // If we just added a fixed method, don't populate the inline cache so that we always take the
        // slow path when overwriting this property and correctly invalidate any JIT-ed code that hard-coded
        // this method.
        if (newDescriptor.IsFixed)
        {
            PropertyValueInfo::SetNoCache(info, instance);
        }
        else
        {
            SetPropertyValueInfo(info, instance, index, attributes);
        }

        if (!IsInternalPropertyId(propertyRecord->GetPropertyId()) && ((this->GetFlags() & IsPrototypeFlag)
            || JavascriptOperators::HasProxyOrPrototypeInlineCacheProperty(instance, propertyRecord->GetPropertyId())))
        {
            // We don't evolve dictionary types when adding a field, so we need to invalidate prototype caches.
            // We only have to do this though if the current type is used as a prototype, or the current property
            // is found on the prototype chain.
            scriptContext->InvalidateProtoCaches(propertyRecord->GetPropertyId());
        }
        SetPropertyUpdateSideEffect(instance, propertyRecord->GetPropertyId(), value, possibleSideEffects);
        return true;
    }

    //
    // Converts (upgrades) this dictionary type handler to an ES5 array type handler. The new handler takes
    // over all members of this handler including the property map.
    //
    template <typename T>
    ES5ArrayTypeHandlerBase<T>* DictionaryTypeHandlerBase<T>::ConvertToES5ArrayType(DynamicObject *instance)
    {
        Recycler* recycler = instance->GetRecycler();

        ES5ArrayTypeHandlerBase<T>* newTypeHandler = RecyclerNew(recycler, ES5ArrayTypeHandlerBase<T>, recycler, this);
        // Don't need to transfer the singleton instance, because the new handler takes over this handler.
        AssertMsg((newTypeHandler->GetFlags() & IsPrototypeFlag) == 0, "Why did we create a brand new type handler with a prototype flag set?");
        newTypeHandler->SetFlags(IsPrototypeFlag, this->GetFlags());
        // Property types were copied in the constructor.
        //newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection | PropertyTypesInlineSlotCapacityLocked, this->GetPropertyTypes());
        newTypeHandler->SetInstanceTypeHandler(instance);
        return newTypeHandler;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::SetAllPropertiesToUndefined(DynamicObject* instance, bool invalidateFixedFields)
    {
        // The Var for window is reused across navigation. we shouldn't preserve the IsExtensibleFlag when we don't keep
        // the expandoes. Reset the IsExtensibleFlag in cleanup scenario should be good enough
        // to cover all the preventExtension/Freeze/Seal scenarios.
        // Note that we don't change the flag for keepProperties scenario: the flags should be preserved and that's consistent
        // with other browsers.
        ChangeFlags(IsExtensibleFlag | IsSealedOnceFlag | IsFrozenOnceFlag, IsExtensibleFlag);

        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.
        int propertyCount = this->propertyMap->Count();

        if (invalidateFixedFields)
        {
            for (int propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
            {
                const PropertyRecord* propertyRecord = this->propertyMap->GetKeyAt(propertyIndex);
                DictionaryPropertyDescriptor<T>* descriptor = this->propertyMap->GetReferenceAt(propertyIndex);
                InvalidateFixedField(instance, propertyRecord->GetPropertyId(), descriptor);
            }
        }

        Js::RecyclableObject* undefined = instance->GetLibrary()->GetUndefined();
        Js::JavascriptFunction* defaultAccessor = instance->GetLibrary()->GetDefaultAccessorFunction();
        for (int propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
        {
            DictionaryPropertyDescriptor<T>* descriptor = this->propertyMap->GetReferenceAt(propertyIndex);

            T dataPropertyIndex = descriptor->GetDataPropertyIndex<false>();
            if (dataPropertyIndex != NoSlots)
            {
                SetSlotUnchecked(instance, dataPropertyIndex, undefined);
            }
            else
            {
                SetSlotUnchecked(instance, descriptor->GetGetterPropertyIndex(), defaultAccessor);
                SetSlotUnchecked(instance, descriptor->GetSetterPropertyIndex(), defaultAccessor);
            }
        }
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::MarshalAllPropertiesToScriptContext(DynamicObject* instance, ScriptContext* targetScriptContext, bool invalidateFixedFields)
    {
        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.
        if (invalidateFixedFields)
        {
            int propertyCount = this->propertyMap->Count();
            for (int propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
            {
                const PropertyRecord* propertyRecord = this->propertyMap->GetKeyAt(propertyIndex);
                DictionaryPropertyDescriptor<T>* descriptor = this->propertyMap->GetReferenceAt(propertyIndex);
                InvalidateFixedField(instance, propertyRecord->GetPropertyId(), descriptor);
            }
        }

        int slotCount = this->nextPropertyIndex;
        for (int slotIndex = 0; slotIndex < slotCount; slotIndex++)
        {
            SetSlotUnchecked(instance, slotIndex, CrossSite::MarshalVar(targetScriptContext, GetSlot(instance, slotIndex)));
        }
    }

    template <typename T>
    DynamicTypeHandler* DictionaryTypeHandlerBase<T>::ConvertToTypeWithItemAttributes(DynamicObject* instance)
    {
        return JavascriptArray::Is(instance) ? ConvertToES5ArrayType(instance) : this;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::SetIsPrototype(DynamicObject* instance)
    {
        // Don't return if IsPrototypeFlag is set, because we may still need to do a type transition and
        // set fixed bits.  If this handler were to be shared, this instance may not be a prototype yet.
        // We might need to convert to a non-shared type handler and/or change type.
        if (!ChangeTypeOnProto() && !(GetIsOrMayBecomeShared() && IsolatePrototypes()))
        {
            SetFlags(IsPrototypeFlag);
            return;
        }

        Assert(!GetIsShared() || this->singletonInstance == nullptr);
        Assert(this->singletonInstance == nullptr || this->singletonInstance->Get() == instance);

        // Review (jedmiad): Why isn't this getting inlined?
        const auto setFixedFlags = [instance](const PropertyRecord* propertyRecord, DictionaryPropertyDescriptor<T>* const descriptor, bool hasNewType)
        {
            if (IsInternalPropertyId(propertyRecord->GetPropertyId()))
            {
                return;
            }
            if (!(descriptor->Attributes & PropertyDeleted))
            {
                // See PathTypeHandlerBase::ConvertToSimpleDictionaryType for rules governing fixed field bits during type
                // handler transitions.  In addition, we know that the current instance is not yet a prototype.

                Assert(descriptor->SanityCheckFixedBits());
                if (descriptor->IsInitialized)
                {
                    // Since DictionaryTypeHandlers are never shared, we can set fixed fields and clear used as fixed as long
                    // as we have changed the type.  Otherwise populated load field caches would still be valid and would need
                    // to be explicitly invalidated if the property value changes.
                    if (hasNewType)
                    {
                        T dataSlot = descriptor->GetDataPropertyIndex<false>();
                        if (dataSlot != NoSlots)
                        {
                            Var value = instance->GetSlot(dataSlot);
                            // Because DictionaryTypeHandlers are never shared we should always have a property value if the handler
                            // says it's initialized.
                            Assert(value != nullptr);
                            descriptor->IsFixed = (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : (ShouldFixDataProperties() && CheckHeuristicsForFixedDataProps(instance, propertyRecord, value)));
                        }
                        else if (descriptor->IsAccessor)
                        {
                            Assert(descriptor->GetGetterPropertyIndex() != NoSlots && descriptor->GetSetterPropertyIndex() != NoSlots);
                            descriptor->IsFixed = ShouldFixAccessorProperties();
                        }

                        // Since we have a new type we can clear all used as fixed bits.  That's because any instance field loads
                        // will have been invalidated by the type transition, and there are no proto fields loads from this object
                        // because it is just now becoming a proto.
                        descriptor->UsedAsFixed = false;
                    }
                }
                else
                {
                    Assert(!descriptor->IsFixed && !descriptor->UsedAsFixed);
                }
                Assert(descriptor->SanityCheckFixedBits());
            }
        };

        // DictionaryTypeHandlers are never shared. If we allow sharing, we will have to handle this case
        // just like SimpleDictionaryTypeHandler.
        Assert(!GetIsOrMayBecomeShared());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        DynamicType* oldType = instance->GetDynamicType();
        RecyclerWeakReference<DynamicObject>* oldSingletonInstance = GetSingletonInstance();
        TraceFixedFieldsBeforeSetIsProto(instance, this, oldType, oldSingletonInstance);
#endif

        bool hasNewType = false;
        if (ChangeTypeOnProto())
        {
            // Forcing a type transition allows us to fix all fields (even those that were previously marked as non-fixed).
            instance->ChangeType();
            Assert(!instance->HasSharedType());
            hasNewType = true;
        }

        // Currently there is no way to become the prototype if you are a stack instance
        Assert(!ThreadContext::IsOnStack(instance));
        if (AreSingletonInstancesNeeded() && this->singletonInstance == nullptr)
        {
            this->singletonInstance = instance->CreateWeakReferenceToSelf();
        }

        // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
        if (!instance->IsExternal())
        {
            // The propertyMap dictionary is guaranteed to have contiguous entries because we never remove entries from it.
            for (int i = 0; i < propertyMap->Count(); i++)
            {
                const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(i);
                DictionaryPropertyDescriptor<T>* const descriptor = propertyMap->GetReferenceAt(i);
                setFixedFlags(propertyRecord, descriptor, hasNewType);
            }
        }

        SetFlags(IsPrototypeFlag);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        TraceFixedFieldsAfterSetIsProto(instance, this, this, oldType, oldSingletonInstance);
#endif

    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::HasSingletonInstance() const
    {
        return this->singletonInstance != nullptr;
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::TryUseFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext)
    {
        bool result = TryGetFixedProperty<false, true>(propertyRecord, pProperty, propertyType, requestContext);
        TraceUseFixedProperty(propertyRecord, pProperty, result, L"DictionaryTypeHandler", requestContext);
        return result;
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::TryUseFixedAccessor(PropertyRecord const * propertyRecord, Var * pAccessor, FixedPropertyKind propertyType, bool getter, ScriptContext * requestContext)
    {
        bool result = TryGetFixedAccessor<false, true>(propertyRecord, pAccessor, propertyType, getter, requestContext);
        TraceUseFixedProperty(propertyRecord, pAccessor, result, L"DictionaryTypeHandler", requestContext);
        return result;
    }

#if DBG
    template <typename T>
    bool DictionaryTypeHandlerBase<T>::CanStorePropertyValueDirectly(const DynamicObject* instance, PropertyId propertyId, bool allowLetConst)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T> descriptor;

        // We pass Constants::NoProperty for ActivationObjects for functions with same named formals.
        if (propertyId == Constants::NoProperty)
        {
            return true;
        }

        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetValue(propertyRecord, &descriptor))
        {
            if (allowLetConst && (descriptor.Attributes & PropertyLetConstGlobal))
            {
                return true;
            }
            else
            {
                return descriptor.IsInitialized && !descriptor.IsFixed;
            }
        }
        else
        {
            AssertMsg(false, "Asking about a property this type handler doesn't know about?");
            return false;
        }
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::CheckFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, ScriptContext * requestContext)
    {
        return TryGetFixedProperty<true, false>(propertyRecord, pProperty, (Js::FixedPropertyKind)(Js::FixedPropertyKind::FixedMethodProperty | Js::FixedPropertyKind::FixedDataProperty), requestContext);
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::HasAnyFixedProperties() const
    {
        for (int i = 0; i < propertyMap->Count(); i++)
        {
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(i);
            if (descriptor.IsFixed)
            {
                return true;
            }
        }
        return false;
    }
#endif

    template <typename T>
    template <bool allowNonExistent, bool markAsUsed>
    bool DictionaryTypeHandlerBase<T>::TryGetFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext)
    {
        // Note: This function is not thread-safe and cannot be called from the JIT thread.  That's why we collect and
        // cache any fixed function instances during work item creation on the main thread.
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        if (localSingletonInstance != nullptr && localSingletonInstance->GetScriptContext() == requestContext)
        {
            DictionaryPropertyDescriptor<T>* descriptor;
            if (propertyMap->TryGetReference(propertyRecord, &descriptor))
            {
                if (descriptor->Attributes & PropertyDeleted || !descriptor->IsFixed)
                {
                    return false;
                }
                T dataSlot = descriptor->GetDataPropertyIndex<false>();
                if (dataSlot != NoSlots)
                {
                    Assert(!IsInternalPropertyId(propertyRecord->GetPropertyId()));
                    Var value = localSingletonInstance->GetSlot(dataSlot);
                    if (value && ((IsFixedMethodProperty(propertyType) && JavascriptFunction::Is(value)) || IsFixedDataProperty(propertyType)))
                    {
                        *pProperty = value;
                        if (markAsUsed)
                        {
                            descriptor->UsedAsFixed = true;
                        }
                        return true;
                    }
                }
            }
            else
            {
                AssertMsg(allowNonExistent, "Trying to get a fixed function instance for a non-existent property?");
            }
        }

        return false;
    }

    template <typename T>
    template <bool allowNonExistent, bool markAsUsed>
    bool DictionaryTypeHandlerBase<T>::TryGetFixedAccessor(PropertyRecord const * propertyRecord, Var * pAccessor, FixedPropertyKind propertyType, bool getter, ScriptContext * requestContext)
    {
        // Note: This function is not thread-safe and cannot be called from the JIT thread.  That's why we collect and
        // cache any fixed function instances during work item creation on the main thread.
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        if (localSingletonInstance != nullptr && localSingletonInstance->GetScriptContext() == requestContext)
        {
            DictionaryPropertyDescriptor<T>* descriptor;
            if (propertyMap->TryGetReference(propertyRecord, &descriptor))
            {
                if (descriptor->Attributes & PropertyDeleted || !descriptor->IsAccessor || !descriptor->IsFixed)
                {
                    return false;
                }

                T accessorSlot = getter ? descriptor->GetGetterPropertyIndex() : descriptor->GetSetterPropertyIndex();
                if (accessorSlot != NoSlots)
                {
                    Assert(!IsInternalPropertyId(propertyRecord->GetPropertyId()));
                    Var value = localSingletonInstance->GetSlot(accessorSlot);
                    if (value && IsFixedAccessorProperty(propertyType) && JavascriptFunction::Is(value))
                    {
                        *pAccessor = value;
                        if (markAsUsed)
                        {
                            descriptor->UsedAsFixed = true;
                        }
                        return true;
                    }
                }
            }
            else
            {
                AssertMsg(allowNonExistent, "Trying to get a fixed function instance for a non-existent property?");
            }
        }

        return false;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::CopySingletonInstance(DynamicObject* instance, DynamicTypeHandler* typeHandler)
    {
        if (this->singletonInstance != nullptr)
        {
            Assert(AreSingletonInstancesNeeded());
            Assert(this->singletonInstance->Get() == instance);
            typeHandler->SetSingletonInstanceUnchecked(this->singletonInstance);
        }
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::InvalidateFixedField(DynamicObject* instance, PropertyId propertyId, DictionaryPropertyDescriptor<T>* descriptor)
    {
        // DictionaryTypeHandlers are never shared, but if they were we would need to invalidate even if
        // there wasn't a singleton instance.  See SimpleDictionaryTypeHandler::InvalidateFixedFields.
        Assert(!GetIsOrMayBecomeShared());
        if (this->singletonInstance != nullptr)
        {
            Assert(this->singletonInstance->Get() == instance);

            // Even if we wrote a new value into this property (overwriting a previously fixed one), we don't
            // consider the new one fixed. This also means that it's ok to populate the inline caches for
            // this property from now on.
            descriptor->IsFixed = false;

            if (descriptor->UsedAsFixed)
            {
                // Invalidate any JIT-ed code that hard coded this method. No need to invalidate
                // any store field inline caches, because they have never been populated.
#if ENABLE_NATIVE_CODEGEN
                instance->GetScriptContext()->GetThreadContext()->InvalidatePropertyGuards(propertyId);
#endif
                descriptor->UsedAsFixed = false;
            }
        }
    }

#if DBG
    template <typename T>
    bool DictionaryTypeHandlerBase<T>::IsLetConstGlobal(DynamicObject* instance, PropertyId propertyId)
    {
        DictionaryPropertyDescriptor<T>* descriptor;
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor) && (descriptor->Attributes & PropertyLetConstGlobal))
        {
            return true;
        }
        return false;
    }
#endif

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::NextLetConstGlobal(int& index, RootObjectBase* instance, const PropertyRecord** propertyRecord, Var* value, bool* isConst)
    {
        for (; index < propertyMap->Count(); index++)
        {
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(index);

            if (descriptor.Attributes & PropertyLetConstGlobal)
            {
                *propertyRecord = propertyMap->GetKeyAt(index);
                *value = instance->GetSlot(descriptor.GetDataPropertyIndex<true>());
                *isConst = (descriptor.Attributes & PropertyConst) != 0;

                index += 1;

                return true;
            }
        }

        return false;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    template <typename T>
    void DictionaryTypeHandlerBase<T>::DumpFixedFields() const {
        for (int i = 0; i < propertyMap->Count(); i++)
        {
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(i);

            const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(i);
            Output::Print(L" %s %d%d%d,", propertyRecord->GetBuffer(),
                descriptor.IsInitialized ? 1 : 0, descriptor.IsFixed ? 1 : 0, descriptor.UsedAsFixed ? 1 : 0);
        }
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::TraceFixedFieldsBeforeTypeHandlerChange(
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

    template <typename T>
    void DictionaryTypeHandlerBase<T>::TraceFixedFieldsAfterTypeHandlerChange(
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
            Output::Print(L"   fixed fields after:");
            newTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
            Output::Flush();
        }
        if (PHASE_VERBOSE_TESTTRACE1(FixMethodPropsPhase))
        {
            Output::Print(L"   type %s, typeHandler %s, old singleton after %s null (%s), new singleton after %s null\n",
                oldTypeHandler != newTypeHandler ? L"changed" : L"unchanged",
                oldType != instance->GetType() ? L"changed" : L"unchanged",
                oldSingletonInstanceBefore == nullptr ? L"==" : L"!=",
                oldSingletonInstanceBefore != oldTypeHandler->GetSingletonInstance() ? L"changed" : L"unchanged",
                newTypeHandler->GetSingletonInstance() == nullptr ? L"==" : L"!=");
            Output::Print(L"   fixed fields after:");
            newTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
            Output::Flush();
        }
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::TraceFixedFieldsBeforeSetIsProto(
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
    {
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {
            Output::Print(L"FixedFields: PathTypeHandler::SetIsPrototype(0x%p):\n", instance);
            Output::Print(L"   before: type = 0x%p, old singleton = 0x%p(0x%p)\n",
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

    template <typename T>
    void DictionaryTypeHandlerBase<T>::TraceFixedFieldsAfterSetIsProto(
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

    template class DictionaryTypeHandlerBase<PropertyIndex>;
    template class DictionaryTypeHandlerBase<BigPropertyIndex>;

    template <bool allowLetConstGlobal>
    PropertyAttributes GetLetConstGlobalPropertyAttributes(PropertyAttributes attributes)
    {
        return (allowLetConstGlobal && (attributes & PropertyLetConstGlobal) != 0) ? (attributes | PropertyWritable) : attributes;
    }
}

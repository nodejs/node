//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

#include "Types\NullTypeHandler.h"
#include "Types\SimpleTypeHandler.h"

namespace Js
{
    template<size_t size>
    SimpleTypeHandler<size>::SimpleTypeHandler(SimpleTypeHandler<size> * typeHandler)
        : DynamicTypeHandler(sizeof(descriptors) / sizeof(SimplePropertyDescriptor),
            typeHandler->GetInlineSlotCapacity(), typeHandler->GetOffsetOfInlineSlots()), propertyCount(typeHandler->propertyCount)
    {
        Assert(typeHandler->GetIsInlineSlotCapacityLocked());
        SetIsInlineSlotCapacityLocked();
        for (int i = 0; i < propertyCount; i++)
        {
            descriptors[i] = typeHandler->descriptors[i];
        }
    }

    template<size_t size>
    SimpleTypeHandler<size>::SimpleTypeHandler(Recycler*) :
         DynamicTypeHandler(sizeof(descriptors) / sizeof(SimplePropertyDescriptor)),
         propertyCount(0)
    {
        SetIsInlineSlotCapacityLocked();
    }

    template<size_t size>
    SimpleTypeHandler<size>::SimpleTypeHandler(const PropertyRecord* id, PropertyAttributes attributes, PropertyTypes propertyTypes, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots) :
        DynamicTypeHandler(sizeof(descriptors) / sizeof(SimplePropertyDescriptor),
        inlineSlotCapacity, offsetOfInlineSlots, DefaultFlags | IsLockedFlag | MayBecomeSharedFlag | IsSharedFlag), propertyCount(1)
    {
        Assert((attributes & PropertyDeleted) == 0);
        descriptors[0].Id = id;
        descriptors[0].Attributes = attributes;

        Assert((propertyTypes & (PropertyTypesAll & ~PropertyTypesWritableDataOnly)) == 0);
        SetPropertyTypes(PropertyTypesWritableDataOnly, propertyTypes);
        SetIsInlineSlotCapacityLocked();
    }

    template<size_t size>

    SimpleTypeHandler<size>::SimpleTypeHandler(SimplePropertyDescriptor const (&SharedFunctionPropertyDescriptors)[size], PropertyTypes propertyTypes, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots) :
         DynamicTypeHandler(sizeof(descriptors) / sizeof(SimplePropertyDescriptor),
         inlineSlotCapacity, offsetOfInlineSlots, DefaultFlags | IsLockedFlag | MayBecomeSharedFlag | IsSharedFlag), propertyCount(size)
    {
        for (size_t i = 0; i < size; i++)
        {
            Assert((SharedFunctionPropertyDescriptors[i].Attributes & PropertyDeleted) == 0);
            descriptors[i].Id = SharedFunctionPropertyDescriptors[i].Id;
            descriptors[i].Attributes = SharedFunctionPropertyDescriptors[i].Attributes;
        }
        Assert((propertyTypes & (PropertyTypesAll & ~PropertyTypesWritableDataOnly)) == 0);
        SetPropertyTypes(PropertyTypesWritableDataOnly, propertyTypes);
        SetIsInlineSlotCapacityLocked();
    }

    template<size_t size>
    SimpleTypeHandler<size> * SimpleTypeHandler<size>::ConvertToNonSharedSimpleType(DynamicObject* instance)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();


        CompileAssert(_countof(descriptors) == size);

        SimpleTypeHandler * newTypeHandler = RecyclerNew(recycler, SimpleTypeHandler, this);

        // Consider: Add support for fixed fields to SimpleTypeHandler when
        // non-shared.  Here we could set the instance as the singleton instance on the newly
        // created handler.

        newTypeHandler->SetFlags(IsPrototypeFlag | HasKnownSlot0Flag, this->GetFlags());
        Assert(newTypeHandler->GetIsInlineSlotCapacityLocked());
        newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection, this->GetPropertyTypes());
        newTypeHandler->SetInstanceTypeHandler(instance);

        return newTypeHandler;
    }

    template<size_t size>
    template <typename T>
    T* SimpleTypeHandler<size>::ConvertToTypeHandler(DynamicObject* instance)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();

#if DBG
        DynamicType* oldType = instance->GetDynamicType();
#endif

        T* newTypeHandler = RecyclerNew(recycler, T, recycler, SimpleTypeHandler<size>::GetSlotCapacity(), GetInlineSlotCapacity(), GetOffsetOfInlineSlots());
        Assert(HasSingletonInstanceOnlyIfNeeded());

        bool const hasSingletonInstance = newTypeHandler->SetSingletonInstanceIfNeeded(instance);
        // If instance has a shared type, the type handler change below will induce a type transition, which
        // guarantees that any existing fast path field stores (which could quietly overwrite a fixed field
        // on this instance) will be invalidated.  It is safe to mark all fields as fixed.
        bool const allowFixedFields = hasSingletonInstance && instance->HasLockedType();

        for (int i = 0; i < propertyCount; i++)
        {
            Var value = instance->GetSlot(i);
            Assert(value != nullptr || IsInternalPropertyId(descriptors[i].Id->GetPropertyId()));
            bool markAsFixed = allowFixedFields && !IsInternalPropertyId(descriptors[i].Id->GetPropertyId()) &&
                (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : false);
            newTypeHandler->Add(descriptors[i].Id, descriptors[i].Attributes, true, markAsFixed, false, scriptContext);
        }

        newTypeHandler->SetFlags(IsPrototypeFlag | HasKnownSlot0Flag, this->GetFlags());
        // We don't expect to convert to a PathTypeHandler.  If we change this later we need to review if we want the new type handler to have locked
        // inline slot capacity, or if we want to allow shrinking of the SimpleTypeHandler's inline slot capacity.
        Assert(!newTypeHandler->IsPathTypeHandler());
        Assert(newTypeHandler->GetIsInlineSlotCapacityLocked());
        newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection, this->GetPropertyTypes());
        newTypeHandler->SetInstanceTypeHandler(instance);

#if DBG
        // If we marked fields as fixed we had better forced a type transition.
        Assert(!allowFixedFields || instance->GetDynamicType() != oldType);
#endif

        return newTypeHandler;
    }

    template<size_t size>
    DictionaryTypeHandler* SimpleTypeHandler<size>::ConvertToDictionaryType(DynamicObject* instance)
    {
        DictionaryTypeHandler* newTypeHandler = ConvertToTypeHandler<DictionaryTypeHandler>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertSimpleToDictionaryCount++;
#endif
        return newTypeHandler;
    }

    template<size_t size>
    SimpleDictionaryTypeHandler* SimpleTypeHandler<size>::ConvertToSimpleDictionaryType(DynamicObject* instance)
    {
        SimpleDictionaryTypeHandler* newTypeHandler = ConvertToTypeHandler<SimpleDictionaryTypeHandler >(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertSimpleToSimpleDictionaryCount++;
#endif
        return newTypeHandler;
    }

    template<size_t size>
    ES5ArrayTypeHandler* SimpleTypeHandler<size>::ConvertToES5ArrayType(DynamicObject* instance)
    {
        ES5ArrayTypeHandler* newTypeHandler = ConvertToTypeHandler<ES5ArrayTypeHandler>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertSimpleToDictionaryCount++;
#endif
        return newTypeHandler;
    }

    template<size_t size>
    int SimpleTypeHandler<size>::GetPropertyCount()
    {
        return propertyCount;
    }

    template<size_t size>
    PropertyId SimpleTypeHandler<size>::GetPropertyId(ScriptContext* scriptContext, PropertyIndex index)
    {
        if (index < propertyCount && !(descriptors[index].Attributes & PropertyDeleted))
        {
            return descriptors[index].Id->GetPropertyId();
        }
        else
        {
            return Constants::NoProperty;
        }
    }

    template<size_t size>
    PropertyId SimpleTypeHandler<size>::GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index)
    {
        if (index < propertyCount && !(descriptors[index].Attributes & PropertyDeleted))
        {
            return descriptors[index].Id->GetPropertyId();
        }
        else
        {
            return Constants::NoProperty;
        }
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyStringName,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        Assert(propertyStringName);
        Assert(propertyId);
        Assert(type);

        for( ; index < propertyCount; ++index )
        {
            PropertyAttributes attribs = descriptors[index].Attributes;
            if( !(attribs & PropertyDeleted) && (!requireEnumerable || (attribs & PropertyEnumerable)))
            {
                const PropertyRecord* propertyRecord = descriptors[index].Id;

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
                PropertyString* propertyString = type->GetScriptContext()->GetPropertyString(*propertyId);
                *propertyStringName = propertyString;
                if (attribs & PropertyWritable)
                {
                    uint16 inlineOrAuxSlotIndex;
                    bool isInlineSlot;
                    PropertyIndexToInlineOrAuxSlotIndex(index, &inlineOrAuxSlotIndex, &isInlineSlot);

                    propertyString->UpdateCache(type, inlineOrAuxSlotIndex, isInlineSlot, true);
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

    template<size_t size>
    PropertyIndex SimpleTypeHandler<size>::GetPropertyIndex(PropertyRecord const* propertyRecord)
    {
        int index;
        if (GetDescriptor(propertyRecord->GetPropertyId(), &index) && !(descriptors[index].Attributes & PropertyDeleted))
        {
            return (PropertyIndex)index;
        }
        return Constants::NoSlot;
    }

    template<size_t size>
    bool SimpleTypeHandler<size>::GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info)
    {
        int index;
        if (GetDescriptor(propertyRecord->GetPropertyId(), &index) && !(descriptors[index].Attributes & PropertyDeleted))
        {
            info.slotIndex = AdjustSlotIndexForInlineSlots((PropertyIndex)index);
            info.isWritable = !!(descriptors[index].Attributes & PropertyWritable);
            return true;
        }
        else
        {
            info.slotIndex = Constants::NoSlot;
            info.isWritable = true;
            return false;
        }
    }

    template<size_t size>
    bool SimpleTypeHandler<size>::IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex)
    {
        Js::EquivalentPropertyEntry* properties = record.properties;
        for (uint pi = 0; pi < record.propertyCount; pi++)
        {
            const EquivalentPropertyEntry* refInfo = &properties[pi];
            if (!this->SimpleTypeHandler<size>::IsObjTypeSpecEquivalent(type, refInfo))
            {
                failedPropertyIndex = pi;
                return false;
            }
        }

        return true;
    }

    template<size_t size>
    bool SimpleTypeHandler<size>::IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry *entry)
    {
        if (this->propertyCount > 0)
        {
            for (int i = 0; i < this->propertyCount; i++)
            {
                SimplePropertyDescriptor* descriptor = &this->descriptors[i];
                Js::PropertyId propertyId = descriptor->Id->GetPropertyId();

                if (entry->propertyId == propertyId && !(descriptor->Attributes & PropertyDeleted))
                {
                    Js::PropertyIndex relSlotIndex = AdjustValidSlotIndexForInlineSlots(static_cast<PropertyIndex>(0));
                    if (relSlotIndex != entry->slotIndex ||
                        entry->isAuxSlot != (GetInlineSlotCapacity() == 0) ||
                        (entry->mustBeWritable && !(descriptor->Attributes & PropertyWritable)))
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


    template<size_t size>
    BOOL SimpleTypeHandler<size>::HasProperty(DynamicObject* instance, PropertyId propertyId, __out_opt bool *noRedecl)
    {
        if (noRedecl != nullptr)
        {
            *noRedecl = false;
        }

        for (int i = 0; i < propertyCount; i++)
        {
            if (descriptors[i].Id->GetPropertyId() == propertyId)
            {
                if (descriptors[i].Attributes & PropertyDeleted)
                {
                    return false;
                }
                if (noRedecl && descriptors[i].Attributes & PropertyNoRedecl)
                {
                    *noRedecl = true;
                }
                return true;
            }
        }

        // Check numeric propertyId only if objectArray available
        uint32 indexVal;
        ScriptContext* scriptContext = instance->GetScriptContext();
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return SimpleTypeHandler<size>::HasItem(instance, indexVal);
        }

        return false;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        for (int i = 0; i < propertyCount; i++)
        {
            if (descriptors[i].Id->Equals(propertyName))
            {
                return !(descriptors[i].Attributes & PropertyDeleted);
            }
        }

        return false;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        for (int i = 0; i < propertyCount; i++)
        {
            if (descriptors[i].Id->GetPropertyId() == propertyId)
            {
                if (descriptors[i].Attributes & PropertyDeleted)
                {
                    return false;
                }
                *value = instance->GetSlot(i);
                PropertyValueInfo::Set(info, instance, static_cast<PropertyIndex>(i), descriptors[i].Attributes);
                return true;
            }
        }

        // Check numeric propertyId only if objectArray available
        uint32 indexVal;
        ScriptContext* scriptContext = instance->GetScriptContext();
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return SimpleTypeHandler<size>::GetItem(instance, originalInstance, indexVal, value, scriptContext);
        }

        return false;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        for (int i = 0; i < propertyCount; i++)
        {
            if (descriptors[i].Id->Equals(propertyName))
            {
                if (descriptors[i].Attributes & PropertyDeleted)
                {
                    return false;
                }
                *value = instance->GetSlot(i);
                PropertyValueInfo::Set(info, instance, static_cast<PropertyIndex>(i), descriptors[i].Attributes);
                return true;
            }
        }

        return false;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        int index;
        if (GetDescriptor(propertyId, &index))
        {
            if (descriptors[index].Attributes & PropertyDeleted)
            {
                // A locked type should not have deleted properties
                Assert(!GetIsLocked());
                descriptors[index].Attributes = PropertyDynamicTypeDefaults;
                instance->SetHasNoEnumerableProperties(false);
            }
            else if (!(descriptors[index].Attributes & PropertyWritable))
            {
                JavascriptError::ThrowCantAssignIfStrictMode(flags, scriptContext);

                PropertyValueInfo::Set(info, instance, static_cast<PropertyIndex>(index), descriptors[index].Attributes); // Try to cache property info even if not writable
                return false;
            }

            SetSlotUnchecked(instance, index, value);
            PropertyValueInfo::Set(info, instance, static_cast<PropertyIndex>(index), descriptors[index].Attributes);
            SetPropertyUpdateSideEffect(instance, propertyId, value, SideEffects_Any);
            return true;
        }

        // Always check numeric propertyId. This may create objectArray.
        uint32 indexVal;
        if (scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return SimpleTypeHandler<size>::SetItem(instance, indexVal, value, flags);
        }

        return this->AddProperty(instance, propertyId, value, PropertyDynamicTypeDefaults, info, flags, SideEffects_Any);
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        // Either the property exists in the dictionary, in which case a PropertyRecord for it exists,
        // or we have to add it to the dictionary, in which case we need to get or create a PropertyRecord.
        // Thus, just get or create one and call the PropertyId overload of SetProperty.
        PropertyRecord const* propertyRecord;
        instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return SimpleTypeHandler<size>::SetProperty(instance, propertyRecord->GetPropertyId(), value, flags, info);
    }

    template<size_t size>
    DescriptorFlags SimpleTypeHandler<size>::GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        int index;
        PropertyValueInfo::SetNoCache(info, instance);
        if (GetDescriptor(propertyId, &index))
        {
            if (descriptors[index].Attributes & PropertyDeleted)
            {
                return None;
            }
            return (descriptors[index].Attributes & PropertyWritable) ? WritableData : Data;
        }

        uint32 indexVal;
        if (instance->GetScriptContext()->IsNumericPropertyId(propertyId, &indexVal))
        {
            return SimpleTypeHandler<size>::GetItemSetter(instance, indexVal, setterValue, requestContext);
        }

        return None;
    }

    template<size_t size>
    DescriptorFlags SimpleTypeHandler<size>::GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        for (int i = 0; i < propertyCount; i++)
        {
            if (descriptors[i].Id->Equals(propertyName))
            {
                if (descriptors[i].Attributes & PropertyDeleted)
                {
                    return None;
                }
                return (descriptors[i].Attributes & PropertyWritable) ? WritableData : Data;
            }
        }

        return None;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        int index;
        if (GetDescriptor(propertyId, &index))
        {
            if (descriptors[index].Attributes & PropertyDeleted)
            {
                // A locked type should not have deleted properties
                Assert(!GetIsLocked());
                return true;
            }
            if (!(descriptors[index].Attributes & PropertyConfigurable))
            {
                JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, scriptContext, scriptContext->GetPropertyName(propertyId)->GetBuffer());

                return false;
            }

            if ((this->GetFlags() & IsPrototypeFlag)
                || JavascriptOperators::HasProxyOrPrototypeInlineCacheProperty(instance, propertyId))
            {
                // We don't evolve dictionary types when deleting a field, so we need to invalidate prototype caches.
                // We only have to do this though if the current type is used as a prototype, or the current property
                // is found on the prototype chain.)
                scriptContext->InvalidateProtoCaches(propertyId);
            }

            instance->ChangeType();


            CompileAssert(_countof(descriptors) == size);
            SetSlotUnchecked(instance, index, nullptr);

            NullTypeHandlerBase* nullTypeHandler = ((this->GetFlags() & IsPrototypeFlag) != 0) ?
                (NullTypeHandlerBase*)NullTypeHandler<true>::GetDefaultInstance() : (NullTypeHandlerBase*)NullTypeHandler<false>::GetDefaultInstance();
            if (instance->HasReadOnlyPropertiesInvisibleToTypeHandler())
            {
                nullTypeHandler->ClearHasOnlyWritableDataProperties();
            }
            SetInstanceTypeHandler(instance, nullTypeHandler, false);

            return true;
        }

        // Check numeric propertyId only if objectArray available
        uint32 indexVal;
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return SimpleTypeHandler<size>::DeleteItem(instance, indexVal, propertyOperationFlags);
        }

        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {
        int index;
        if (!GetDescriptor(propertyId, &index))
        {
            return true;
        }
        return descriptors[index].Attributes & PropertyEnumerable;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {
        int index;
        if (!GetDescriptor(propertyId, &index))
        {
            return true;
        }
        return descriptors[index].Attributes & PropertyWritable;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {
        int index;
        if (!GetDescriptor(propertyId, &index))
        {
            return true;
        }
        return descriptors[index].Attributes & PropertyConfigurable;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        int index;
        if (!GetDescriptor(propertyId, &index))
        {
            // Upgrade type handler if set objectArray item attribute.
            // Only check numeric propertyId if objectArray available.
            ScriptContext* scriptContext = instance->GetScriptContext();
            uint32 indexVal;
            if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
            {
                return SimpleTypeHandler<size>::ConvertToTypeWithItemAttributes(instance)
                    ->SetEnumerable(instance, propertyId, value);
            }
            return true;
        }

        if (value)
        {
            if (SetAttribute(instance, index, PropertyEnumerable))
            {
                instance->SetHasNoEnumerableProperties(false);
            }
        }
        else
        {
            ClearAttribute(instance, index, PropertyEnumerable);
        }
        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        int index;
        if (!GetDescriptor(propertyId, &index))
        {
            // Upgrade type handler if set objectArray item attribute.
            // Only check numeric propertyId if objectArray available.
            ScriptContext* scriptContext = instance->GetScriptContext();
            uint32 indexVal;
            if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
            {
                return SimpleTypeHandler<size>::ConvertToTypeWithItemAttributes(instance)
                    ->SetWritable(instance, propertyId, value);
            }
            return true;
        }

        const Type* oldType = instance->GetType();
        if (value)
        {
            if (SetAttribute(instance, index, PropertyWritable))
            {
                instance->ChangeTypeIf(oldType); // Ensure type change to invalidate caches
            }
        }
        else
        {
            if (ClearAttribute(instance, index, PropertyWritable))
            {
                instance->ChangeTypeIf(oldType); // Ensure type change to invalidate caches

                // Clearing the attribute may have changed the type handler, so make sure
                // we access the current one.
                DynamicTypeHandler* const typeHandler = GetCurrentTypeHandler(instance);
                typeHandler->ClearHasOnlyWritableDataProperties();
                if (typeHandler->GetFlags() & IsPrototypeFlag)
                {
                    instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }
        }
        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        int index;
        if (!GetDescriptor(propertyId, &index))
        {
            // Upgrade type handler if set objectArray item attribute.
            // Only check numeric propertyId if objectArray available.
            ScriptContext* scriptContext = instance->GetScriptContext();
            uint32 indexVal;
            if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
            {
                return SimpleTypeHandler<size>::ConvertToTypeWithItemAttributes(instance)
                    ->SetConfigurable(instance, propertyId, value);
            }
            return true;
        }

        if (value)
        {
            SetAttribute(instance, index, PropertyConfigurable);
        }
        else
        {
            ClearAttribute(instance, index, PropertyConfigurable);
        }
        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::GetDescriptor(PropertyId propertyId, int * index)
    {
        for (int i = 0; i < propertyCount; i++)
        {
            if (descriptors[i].Id->GetPropertyId() == propertyId)
            {
                *index = i;
                return true;
            }
        }
        return false;
    }

    //
    // Set an attribute bit. Return true if change is made.
    //
    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetAttribute(DynamicObject* instance, int index, PropertyAttributes attribute)
    {
        if (descriptors[index].Attributes & PropertyDeleted)
        {
            // A locked type should not have deleted properties
            Assert(!GetIsLocked());
            return false;
        }

        PropertyAttributes attributes = descriptors[index].Attributes;
        attributes |= attribute;
        if (attributes == descriptors[index].Attributes)
        {
            return false;
        }

        // If the type is locked we must force type transition to invalidate any potential property string
        // caches used in snapshot enumeration.
        if (GetIsLocked())
        {
#if DBG
            DynamicType* oldType = instance->GetDynamicType();
#endif
            // This changes TypeHandler, but non-necessarily Type.
            this->ConvertToNonSharedSimpleType(instance)->descriptors[index].Attributes = attributes;
#if DBG
            Assert(!oldType->GetIsLocked() || instance->GetDynamicType() != oldType);
#endif
        }
        else
        {
            descriptors[index].Attributes = attributes;
        }
        return true;
    }

    //
    // Clear an attribute bit. Return true if change is made.
    //
    template<size_t size>
    BOOL SimpleTypeHandler<size>::ClearAttribute(DynamicObject* instance, int index, PropertyAttributes attribute)
    {
        if (descriptors[index].Attributes & PropertyDeleted)
        {
            // A locked type should not have deleted properties
            Assert(!GetIsLocked());
            return false;
        }

        PropertyAttributes attributes = descriptors[index].Attributes;
        attributes &= ~attribute;
        if (attributes == descriptors[index].Attributes)
        {
            return false;
        }

        // If the type is locked we must force type transition to invalidate any potential property string
        // caches used in snapshot enumeration.
        if (GetIsLocked())
        {
#if DBG
            DynamicType* oldType = instance->GetDynamicType();
#endif
            // This changes TypeHandler, but non-necessarily Type.
            this->ConvertToNonSharedSimpleType(instance)->descriptors[index].Attributes = attributes;
#if DBG
            Assert(!oldType->GetIsLocked() || instance->GetDynamicType() != oldType);
#endif
        }
        else
        {
            descriptors[index].Attributes = attributes;
        }
        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {
        return ConvertToDictionaryType(instance)->SetAccessors(instance, propertyId, getter, setter, flags);
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::PreventExtensions(DynamicObject* instance)
    {
        return ConvertToDictionaryType(instance)->PreventExtensions(instance);
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::Seal(DynamicObject* instance)
    {
        return ConvertToDictionaryType(instance)->Seal(instance);
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {
        return ConvertToDictionaryType(instance)->Freeze(instance, true);
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        int index;
        if (GetDescriptor(propertyId, &index))
        {
            if (descriptors[index].Attributes != attributes)
            {
                SimpleTypeHandler * typeHandler = this;
                if (GetIsLocked())
                {
#if DBG
                    DynamicType* oldType = instance->GetDynamicType();
#endif
                    typeHandler = this->ConvertToNonSharedSimpleType(instance);
#if DBG
                    Assert(!oldType->GetIsLocked() || instance->GetDynamicType() != oldType);
#endif
                }
                typeHandler->descriptors[index].Attributes = attributes;
                if (attributes & PropertyEnumerable)
                {
                    instance->SetHasNoEnumerableProperties(false);
                }
                if (!(attributes & PropertyWritable))
                {
                    typeHandler->ClearHasOnlyWritableDataProperties();
                    if (typeHandler->GetFlags() & IsPrototypeFlag)
                    {
                        instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyId);
                        instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                    }
                }
            }
            SetSlotUnchecked(instance, index, value);
            PropertyValueInfo::Set(info, instance, static_cast<PropertyIndex>(index), descriptors[index].Attributes);
            SetPropertyUpdateSideEffect(instance, propertyId, value, possibleSideEffects);
            return true;
        }

        // Always check numeric propertyId. May create objectArray.
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 indexVal;
        if (scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return SimpleTypeHandler<size>::SetItemWithAttributes(instance, indexVal, value, attributes);
        }

        return this->AddProperty(instance, propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {
        for (int i = 0; i < propertyCount; i++)
        {
            if (descriptors[i].Id->GetPropertyId() == propertyId)
            {
                if (descriptors[i].Attributes & PropertyDeleted)
                {
                    return true;
                }

                descriptors[i].Attributes = (descriptors[i].Attributes & ~PropertyDynamicTypeDefaults) | (attributes & PropertyDynamicTypeDefaults);
                if (descriptors[i].Attributes & PropertyEnumerable)
                {
                    instance->SetHasNoEnumerableProperties(false);
                }
                if (!(descriptors[i].Attributes & PropertyWritable))
                {
                    this->ClearHasOnlyWritableDataProperties();
                    if (GetFlags() & IsPrototypeFlag)
                    {
                        instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyId);
                        instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                    }
                }
                return true;
            }
        }

        // Check numeric propertyId only if objectArray available
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 indexVal;
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return SimpleTypeHandler<size>::SetItemAttributes(instance, indexVal, attributes);
        }

        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {
        if (index >= propertyCount) { return false; }
        Assert(descriptors[index].Id->GetPropertyId() == propertyId);
        if (descriptors[index].Attributes & PropertyDeleted)
        {
            return false;
        }
        *attributes = descriptors[index].Attributes & PropertyDynamicTypeDefaults;
        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::AddProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();

#if DBG
        int index;
        uint32 indexVal;
        Assert(!GetDescriptor(propertyId, &index));
        Assert(!scriptContext->IsNumericPropertyId(propertyId, &indexVal));
#endif

        if (propertyCount >= sizeof(descriptors)/sizeof(SimplePropertyDescriptor))
        {
            Assert(propertyId != Constants::NoProperty);
            PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
            return ConvertToSimpleDictionaryType(instance)->AddProperty(instance, propertyRecord, value, attributes, info, flags, possibleSideEffects);
        }

        descriptors[propertyCount].Id = scriptContext->GetPropertyName(propertyId);
        descriptors[propertyCount].Attributes = attributes;
        if (attributes & PropertyEnumerable)
        {
            instance->SetHasNoEnumerableProperties(false);
        }
        if (!(attributes & PropertyWritable))
        {
            this->ClearHasOnlyWritableDataProperties();
            if (GetFlags() & IsPrototypeFlag)
            {
                instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyId);
                instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
        }
        SetSlotUnchecked(instance, propertyCount, value);
        PropertyValueInfo::Set(info, instance, static_cast<PropertyIndex>(propertyCount), attributes);
        propertyCount++;

        if ((this->GetFlags() && IsPrototypeFlag)
            || JavascriptOperators::HasProxyOrPrototypeInlineCacheProperty(instance, propertyId))
        {
            scriptContext->InvalidateProtoCaches(propertyId);
        }
        SetPropertyUpdateSideEffect(instance, propertyId, value, possibleSideEffects);
        return true;
    }

    template<size_t size>
    void SimpleTypeHandler<size>::SetAllPropertiesToUndefined(DynamicObject* instance, bool invalidateFixedFields)
    {
        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.

        // We can ignore invalidateFixedFields, because SimpleTypeHandler doesn't support fixed fields at this point.
        Js::RecyclableObject* undefined = instance->GetLibrary()->GetUndefined();
        for (int propertyIndex = 0; propertyIndex < this->propertyCount; propertyIndex++)
        {
            SetSlotUnchecked(instance, propertyIndex, undefined);
        }
    }

    template<size_t size>
    void SimpleTypeHandler<size>::MarshalAllPropertiesToScriptContext(DynamicObject* instance, ScriptContext* targetScriptContext, bool invalidateFixedFields)
    {
        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.

        // We can ignore invalidateFixedFields, because SimpleTypeHandler doesn't support fixed fields at this point.
        for (int propertyIndex = 0; propertyIndex < this->propertyCount; propertyIndex++)
        {
            SetSlotUnchecked(instance, propertyIndex, CrossSite::MarshalVar(targetScriptContext, GetSlot(instance, propertyIndex)));
        }
    }

    template<size_t size>
    DynamicTypeHandler* SimpleTypeHandler<size>::ConvertToTypeWithItemAttributes(DynamicObject* instance)
    {
        return JavascriptArray::Is(instance) ?
            ConvertToES5ArrayType(instance) : ConvertToDictionaryType(instance);
    }

    template<size_t size>
    void SimpleTypeHandler<size>::SetIsPrototype(DynamicObject* instance)
    {
        // Don't return if IsPrototypeFlag is set, because we may still need to do a type transition and
        // set fixed bits.  If this handler is shared, this instance may not even be a prototype yet.
        // In this case we may need to convert to a non-shared type handler.
        if (!ChangeTypeOnProto() && !(GetIsOrMayBecomeShared() && IsolatePrototypes()))
        {
            return;
        }

        ConvertToSimpleDictionaryType(instance)->SetIsPrototype(instance);
    }

#if DBG
    template<size_t size>
    bool SimpleTypeHandler<size>::CanStorePropertyValueDirectly(const DynamicObject* instance, PropertyId propertyId, bool allowLetConst)
    {
        Assert(!allowLetConst);
        int index;
        if (GetDescriptor(propertyId, &index))
        {
            return true;
        }
        else
        {
            AssertMsg(false, "Asking about a property this type handler doesn't know about?");
            return false;
        }
    }
#endif

    template class SimpleTypeHandler<1>;
    template class SimpleTypeHandler<2>;

}

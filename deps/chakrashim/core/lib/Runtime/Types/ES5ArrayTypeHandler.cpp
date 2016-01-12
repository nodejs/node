//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

#include "Library\ES5Array.h"

namespace Js
{
    IndexPropertyDescriptorMap::IndexPropertyDescriptorMap(Recycler* recycler)
        : recycler(recycler), indexList(NULL), lastIndexAt(-1)
    {
        indexPropertyMap = RecyclerNew(recycler, InnerMap, recycler);
    }

    void IndexPropertyDescriptorMap::Add(uint32 key, const IndexPropertyDescriptor& value)
    {
        if (indexPropertyMap->Count() >= (INT_MAX / 2))
        {
            Js::Throw::OutOfMemory(); // Would possibly overflow our dictionary
        }

        indexList = NULL; // Discard indexList on change
        indexPropertyMap->Add(key, value);
    }

    //
    // Build sorted index list if not found.
    //
    void IndexPropertyDescriptorMap::EnsureIndexList()
    {
        if (!indexList)
        {
            int length = Count();
            indexList = RecyclerNewArrayLeaf(recycler, uint32, length);
            lastIndexAt = -1; // Reset lastAccessorIndexAt

            for (int i = 0; i < length; i++)
            {
                indexList[i] = GetKeyAt(i);
            }

            ::qsort(indexList, length, sizeof(uint32), &CompareIndex);
        }
    }

    //
    // Try get the last index in this map if it contains any valid index.
    //
    bool IndexPropertyDescriptorMap::TryGetLastIndex(uint32* lastIndex)
    {
        if (Count() == 0)
        {
            return false;
        }

        EnsureIndexList();

        // Search the index list backwards for the last index
        for (int i = Count() - 1; i >= 0; i--)
        {
            uint32 key = indexList[i];

            IndexPropertyDescriptor* descriptor;
            bool b = TryGetReference(key, &descriptor);
            Assert(b && descriptor);

            if (!(descriptor->Attributes & PropertyDeleted))
            {
                *lastIndex = key;
                return true;
            }
        }

        return false;
    }

    //
    // Get the next index in the map, similar to JavascriptArray::GetNextIndex().
    //
    BOOL IndexPropertyDescriptorMap::IsValidDescriptorToken(void * descriptorValidationToken) const
    {
        return indexList != nullptr && descriptorValidationToken == indexList;
    }

    uint32 IndexPropertyDescriptorMap::GetNextDescriptor(uint32 key, IndexPropertyDescriptor** ppDescriptor, void ** pDescriptorValidationToken)
    {
        *pDescriptorValidationToken = nullptr;
        *ppDescriptor = nullptr;
        if (Count() == 0)
        {
            return JavascriptArray::InvalidIndex;
        }

        EnsureIndexList();

        // Find the first item index > key
        int low = 0;
        if (key != JavascriptArray::InvalidIndex)
        {
            Assert(lastIndexAt < Count()); // lastIndexAt must be either -1 or in range [0, Count)
            if (lastIndexAt >= 0 && indexList[lastIndexAt] == key)
            {
                low = lastIndexAt + 1;
            }
            else
            {
                int high = Count() - 1;
                while (low < high)
                {
                    int mid = (low + high) / 2;
                    if (indexList[mid] <= key)
                    {
                        low = mid + 1;
                    }
                    else
                    {
                        high = mid;
                    }
                }
                if (low < Count() && indexList[low] <= key)
                {
                    ++low;
                }
            }
        }

        // Search for the next valid index
        for (; low < Count(); low++)
        {
            uint32 index = indexList[low];
            IndexPropertyDescriptor* descriptor;
            bool b = TryGetReference(index, &descriptor);
            Assert(b && descriptor);

            if (!(descriptor->Attributes & PropertyDeleted))
            {
                lastIndexAt = low; // Save last index location
                *pDescriptorValidationToken = indexList; // use the index list to keep track of where the descriptor has been changed.
                *ppDescriptor = descriptor;
                return index;
            }
        }

        return JavascriptArray::InvalidIndex;
    }

    //
    // Try to delete the range [firstKey, length) from right to left, stop if running into an element whose
    // [[CanDelete]] is false. Return the index where [index, ...) are all deleted.
    //
    uint32 IndexPropertyDescriptorMap::DeleteDownTo(uint32 firstKey)
    {
        EnsureIndexList();

        // Iterate the index list backwards to delete from right to left
        for (int i = Count() - 1; i >= 0; i--)
        {
            uint32 key = indexList[i];
            if (key < firstKey)
            {
                break; // We are done, [firstKey, ...) have already been deleted
            }

            IndexPropertyDescriptor* descriptor;
            bool b = TryGetReference(key, &descriptor);
            Assert(b && descriptor);

            if (descriptor->Attributes & PropertyDeleted)
            {
                continue; // Skip empty entry
            }

            if (descriptor->Attributes & PropertyConfigurable)
            {
                descriptor->Getter = NULL;
                descriptor->Setter = NULL;
                descriptor->Attributes = PropertyDeleted | PropertyWritable | PropertyConfigurable;
            }
            else
            {
                // Cannot delete key, and [key + 1, ...) are all deleted
                return key + 1;
            }
        }

        return firstKey;
    }

    template <class T>
    ES5ArrayTypeHandlerBase<T>* ES5ArrayTypeHandlerBase<T>::New(Recycler * recycler, int initialCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots)
    {
        return NewTypeHandler<ES5ArrayTypeHandlerBase<T>>(recycler, initialCapacity, inlineSlotCapacity, offsetOfInlineSlots);
    }

    template <class T>
    ES5ArrayTypeHandlerBase<T>::ES5ArrayTypeHandlerBase(Recycler* recycler)
        : DictionaryTypeHandlerBase<T>(recycler), dataItemAttributes(PropertyDynamicTypeDefaults), lengthWritable(true)
    {
        indexPropertyMap = RecyclerNew(recycler, IndexPropertyDescriptorMap, recycler);
    }

    template <class T>
    ES5ArrayTypeHandlerBase<T>::ES5ArrayTypeHandlerBase(Recycler* recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots)
        : DictionaryTypeHandlerBase<T>(recycler, slotCapacity, inlineSlotCapacity, offsetOfInlineSlots), dataItemAttributes(PropertyDynamicTypeDefaults), lengthWritable(true)
    {
        indexPropertyMap = RecyclerNew(recycler, IndexPropertyDescriptorMap, recycler);
    }

    template <class T>
    ES5ArrayTypeHandlerBase<T>::ES5ArrayTypeHandlerBase(Recycler* recycler, DictionaryTypeHandlerBase<T>* typeHandler)
        : DictionaryTypeHandlerBase<T>(typeHandler), dataItemAttributes(PropertyDynamicTypeDefaults), lengthWritable(true)
    {
        indexPropertyMap = RecyclerNew(recycler, IndexPropertyDescriptorMap, recycler);
    }

    template <class T>
    void ES5ArrayTypeHandlerBase<T>::SetIsPrototype(DynamicObject * instance)
    {
        __super::SetIsPrototype(instance);

        // We have ES5 array has array/object prototype, we can't use array fast path for set
        // as index could be readonly or be getter/setter in the prototype
        // TODO: we may be able to separate out the array fast path and the object array fast path
        // here.
        instance->GetScriptContext()->optimizationOverrides.DisableArraySetElementFastPath();
    }

    template <class T>
    void ES5ArrayTypeHandlerBase<T>::SetInstanceTypeHandler(DynamicObject* instance, bool hasChanged)
    {
        Assert(JavascriptArray::Is(instance));
        if (this->GetFlags() & DynamicTypeHandler::IsPrototypeFlag)
        {
            // We have ES5 array has array/object prototype, we can't use array fast path for set
            // as index could be readonly or be getter/setter in the prototype
            // TODO: we may be able to separate out the array fast path and the object array fast path
            // here.
            instance->GetScriptContext()->optimizationOverrides.DisableArraySetElementFastPath();
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        JavascriptArray * arrayInstance = JavascriptArray::EnsureNonNativeArray(JavascriptArray::FromVar(instance));
#if DBG
        bool doneConversion = false;
        Js::Type* oldType = arrayInstance->GetType();
#endif
        bool isCrossSiteObject = false;
        __try
        {
            if (!CrossSite::IsCrossSiteObjectTyped(arrayInstance))
            {
                // Convert instance to an ES5Array
                Assert(VirtualTableInfo<JavascriptArray>::HasVirtualTable(arrayInstance));
                VirtualTableInfo<ES5Array>::SetVirtualTable(arrayInstance);
            }
            else
            {
                // If instance was a cross-site JavascriptArray, convert to a cross-site ES5Array
                Assert(VirtualTableInfo<CrossSiteObject<JavascriptArray>>::HasVirtualTable(arrayInstance));
                VirtualTableInfo<CrossSiteObject<ES5Array>>::SetVirtualTable(arrayInstance);
                isCrossSiteObject = true;
            }

            arrayInstance->ChangeType(); // force change TypeId
            __super::SetInstanceTypeHandler(arrayInstance, false); // after forcing the type change, we don't need to changeType again.
#if DBG
            doneConversion = true;
#endif
        }
        __finally
        {
            if (AbnormalTermination())
            {
                Assert(!doneConversion);
                // change vtbl shouldn't OOM. revert back the vtable.
                if (isCrossSiteObject)
                {
                    Assert(VirtualTableInfo<CrossSiteObject<ES5Array>>::HasVirtualTable(arrayInstance));
                    VirtualTableInfo<CrossSiteObject<JavascriptArray>>::SetVirtualTable(arrayInstance);
                }
                else
                {
                    Assert(VirtualTableInfo<ES5Array>::HasVirtualTable(arrayInstance));
                    VirtualTableInfo<JavascriptArray>::SetVirtualTable(arrayInstance);
                }
                // The only allocation is in ChangeType, which won't have changed the type yet.
                Assert(arrayInstance->GetType() == oldType);
            }
        }
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::HasDataItem(ES5Array* arr, uint32 index)
    {
        Var value;
        return arr->DirectGetItemAt(index, &value);
    }

    //
    // Check if the array contains any data item not in attribute map (so that we know there are items
    // using shared data item attributes)
    //
    template <class T>
    bool ES5ArrayTypeHandlerBase<T>::HasAnyDataItemNotInMap(ES5Array* arr)
    {
        JavascriptArray::ArrayElementEnumerator e(arr);
        while (e.MoveNext<Var>())
        {
            if (!indexPropertyMap->ContainsKey(e.GetIndex()))
            {
                return true;
            }
        }

        return false;
    }

    template <class T>
    PropertyAttributes ES5ArrayTypeHandlerBase<T>::GetDataItemAttributes() const
    {
        return dataItemAttributes;
    }
    template <class T>
    void ES5ArrayTypeHandlerBase<T>::SetDataItemSealed()
    {
        dataItemAttributes &= ~(PropertyConfigurable);
    }
    template <class T>
    void ES5ArrayTypeHandlerBase<T>::SetDataItemFrozen()
    {
        dataItemAttributes &= ~(PropertyWritable | PropertyConfigurable);
        this->ClearHasOnlyWritableDataProperties();
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::CantAssign(PropertyOperationFlags flags, ScriptContext* scriptContext)
    {
        JavascriptError::ThrowCantAssignIfStrictMode(flags, scriptContext);
        return false;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::CantExtend(PropertyOperationFlags flags, ScriptContext* scriptContext)
    {
        JavascriptError::ThrowCantExtendIfStrictMode(flags, scriptContext);
        return false;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::HasItem(ES5Array* arr, uint32 index)
    {
        // We have the item if we have its descriptor.
        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {
            return !(descriptor->Attributes & PropertyDeleted);
        }

        // Otherwise check if we have such a data item.
        return HasDataItem(arr, index);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItem(ES5Array* arr, DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();

        // Reject if we need to grow non-writable length
        if (!CanSetItemAt(arr, index))
        {
            return false;
        }

        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                if (!(this->GetFlags() & IsExtensibleFlag))
                {
                    return CantExtend(flags, scriptContext);
                }

                // No need to change hasNoEnumerableProperties. See comment in ES5ArrayTypeHandlerBase<T>::SetItemWithAttributes.
                Assert(!arr->GetHasNoEnumerableProperties());

                Assert(!descriptor->Getter && !descriptor->Setter);
                descriptor->Attributes = PropertyDynamicTypeDefaults;
                arr->DirectSetItemAt(index, value);
                return true;
            }

            if (!(descriptor->Attributes & PropertyWritable))
            {
                return CantAssign(flags, scriptContext);
            }

            if (HasDataItem(arr, index))
            {
                arr->DirectSetItemAt(index, value);
            }
            else if (descriptor->Setter)
            {
                RecyclableObject* func = RecyclableObject::FromVar(descriptor->Setter);
                // TODO : request context
                JavascriptOperators::CallSetter(func, instance, value, NULL);
            }

            return true;
        }

        //
        // Not found in attribute map. Extend or update data item.
        //
        if (!(this->GetFlags() & IsExtensibleFlag))
        {
            if (!HasDataItem(arr, index))
            {
                return CantExtend(flags, scriptContext);
            }
            else if (!(GetDataItemAttributes() & PropertyWritable))
            {
                return CantAssign(flags, scriptContext);
            }
        }

        // No need to change hasNoEnumerableProperties. See comment in ES5ArrayTypeHandlerBase<T>::SetItemWithAttributes.
        Assert(!arr->GetHasNoEnumerableProperties());

        arr->DirectSetItemAt(index, value); // sharing data item attributes
        return true;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItemWithAttributes(ES5Array* arr, DynamicObject* instance, uint32 index, Var value, PropertyAttributes attributes)
    {
        // Reject if we need to grow non-writable length
        if (!CanSetItemAt(arr, index))
        {
            return false;
        }

        // We don't track non-enumerable items in object array.  Objects with an object array
        // report having enumerable properties.  See DynamicObject::GetHasNoEnumerableProperties.
        // Array objects (which don't have an object array, and could report their hasNoEnumerableProperties
        // directly) take an explicit type transition before switching to ES5ArrayTypeHandler, so their
        // hasNoEnumerableProperties flag gets cleared.
        Assert(!arr->GetHasNoEnumerableProperties());

        if (!(attributes & PropertyWritable))
        {
            this->ClearHasOnlyWritableDataProperties();
            if(GetFlags() & IsPrototypeFlag)
            {
                instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
        }

        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                Assert(!descriptor->Getter && !descriptor->Setter);
                descriptor->Attributes = attributes;
                arr->DirectSetItemAt(index, value);
                return true;
            }

            descriptor->Attributes = attributes;

            if (HasDataItem(arr, index))
            {
                arr->DirectSetItemAt(index, value);
            }
            else if (descriptor->Setter)
            {
                RecyclableObject* func = RecyclableObject::FromVar(descriptor->Setter);
                // TODO : request context
                JavascriptOperators::CallSetter(func, instance, value, NULL);
            }
        }
        else
        {
            // See comment for the same assert above.
            Assert(!arr->GetHasNoEnumerableProperties());

            // Not found in attribute map
            arr->DirectSetItemAt(index, value);
            indexPropertyMap->Add(index, IndexPropertyDescriptor(attributes));
        }

        return true;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItemAttributes(ES5Array* arr, DynamicObject* instance, uint32 index, PropertyAttributes attributes)
    {
        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                return false;
            }

            // No need to change hasNoEnumerableProperties. See comment in ES5ArrayTypeHandlerBase<T>::SetItemWithAttributes.
            Assert(!arr->GetHasNoEnumerableProperties());

            descriptor->Attributes = (descriptor->Attributes & ~PropertyDynamicTypeDefaults) | (attributes & PropertyDynamicTypeDefaults);
            if (!(descriptor->Attributes & PropertyWritable))
            {
                this->ClearHasOnlyWritableDataProperties();
                if(GetFlags() & IsPrototypeFlag)
                {
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }
            return true;
        }
        else if (HasDataItem(arr, index))
        {
            // No need to change hasNoEnumerableProperties. See comment in ES5ArrayTypeHandlerBase<T>::SetItemWithAttributes.
            Assert(!arr->GetHasNoEnumerableProperties());

            indexPropertyMap->Add(index, IndexPropertyDescriptor(attributes & PropertyDynamicTypeDefaults));
            if (!(attributes & PropertyWritable))
            {
                this->ClearHasOnlyWritableDataProperties();
                if(GetFlags() & IsPrototypeFlag)
                {
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }
            return true;
        }

        return false;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItemAccessors(ES5Array* arr, DynamicObject* instance, uint32 index, Var getter, Var setter)
    {
        // Reject if we need to grow non-writable length
        if (!CanSetItemAt(arr, index))
        {
            return false;
        }

        JavascriptLibrary* lib = instance->GetLibrary();
        if (getter)
        {
            getter = CanonicalizeAccessor(getter, lib);
        }
        if (setter)
        {
            setter = CanonicalizeAccessor(setter, lib);
        }

        // conversion from data-property to accessor property
        arr->DirectDeleteItemAt<Var>(index);

        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                descriptor->Attributes = PropertyDynamicTypeDefaults;
            }
            if (getter)
            {
                descriptor->Getter = getter;
            }
            if (setter)
            {
                descriptor->Setter = setter;
            }
        }
        else
        {
            indexPropertyMap->Add(index, IndexPropertyDescriptor(getter, setter));
        }

        if (arr->GetLength() <= index)
        {
            uint32 newLength = index;
            UInt32Math::Inc(newLength);
            arr->SetLength(newLength);
        }

        this->ClearHasOnlyWritableDataProperties();
        if(GetFlags() & IsPrototypeFlag)
        {
            instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }
        return true;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetItemAccessors(ES5Array* arr, DynamicObject* instance, uint32 index, Var* getter, Var* setter)
    {
        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                return false;
            }

            if (!HasDataItem(arr, index)) // if not shadowed by data item
            {
                *getter = descriptor->Getter;
                *setter = descriptor->Setter;
                return descriptor->Getter || descriptor->Setter;
            }
        }

        return false;
    }

    // Check if this array can set item at the given index.
    template <class T>
    bool ES5ArrayTypeHandlerBase<T>::CanSetItemAt(ES5Array* arr, uint32 index) const
    {
        return IsLengthWritable() || index < arr->GetLength();
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::DeleteItem(ES5Array* arr, DynamicObject* instance, uint32 index, PropertyOperationFlags propertyOperationFlags)
    {
        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                return true;
            }
            else if (!(descriptor->Attributes & PropertyConfigurable))
            {
                JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, instance->GetScriptContext(), TaggedInt::ToString(index, instance->GetScriptContext())->GetString());

                return false;
            }

            arr->DirectDeleteItemAt<Var>(index);
            descriptor->Getter = NULL;
            descriptor->Setter = NULL;
            descriptor->Attributes = PropertyDeleted | PropertyWritable | PropertyConfigurable;
            return true;
        }

        // Not in attribute map
        if (!(GetDataItemAttributes() & PropertyConfigurable))
        {
            return !HasDataItem(arr, index); // CantDelete
        }
        return arr->DirectDeleteItemAt<Var>(index);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetItem(ES5Array* arr, DynamicObject* instance, Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {
        if (arr->DirectGetItemAt<Var>(index, value))
        {
            return true;
        }

        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                return false;
            }

            if (descriptor->Getter)
            {
                RecyclableObject* func = RecyclableObject::FromVar(descriptor->Getter);
                *value = Js::JavascriptOperators::CallGetter(func, originalInstance, requestContext);
            }
            else
            {
                *value = instance->GetLibrary()->GetUndefined();
            }
            return true;
        }

        return false;
    }

    template <class T>
    DescriptorFlags ES5ArrayTypeHandlerBase<T>::GetItemSetter(ES5Array* arr, DynamicObject* instance, uint32 index, Var* setterValue, ScriptContext* requestContext)
    {
        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {
            if (descriptor->Attributes & PropertyDeleted)
            {
                return None;
            }

            if (HasDataItem(ES5Array::FromVar(instance), index))
            {
                // not a setter but shadows
                return (descriptor->Attributes & PropertyWritable) ? WritableData : Data;
            }
            else if (descriptor->Setter)
            {
                *setterValue = descriptor->Setter;
                return Accessor;
            }
        }
        else if (HasDataItem(ES5Array::FromVar(instance), index))
        {
            return (GetDataItemAttributes() & PropertyWritable) ? WritableData : Data;
        }

        return None;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::HasProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 index;

        if (noRedecl != nullptr)
        {
            *noRedecl = false;
        }

        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            // Call my version of HasItem
            return ES5ArrayTypeHandlerBase<T>::HasItem(instance, index);
        }

        return __super::HasProperty(instance, propertyId, noRedecl);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        return __super::HasProperty(instance, propertyNameString);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            return GetItem(ES5Array::FromVar(instance), instance, index, value, requestContext);
        }

        return __super::GetProperty(instance, originalInstance, propertyId, value, info, requestContext);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        return __super::GetProperty(instance, originalInstance, propertyNameString, value, info, requestContext);
    }

    template <class T>
    DescriptorFlags ES5ArrayTypeHandlerBase<T>::GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            PropertyValueInfo::SetNoCache(info, instance);
            return ES5ArrayTypeHandlerBase<T>::GetItemSetter(instance, index, setterValue, requestContext);
        }

        return __super::GetSetter(instance, propertyId, setterValue, info, requestContext);
    }

    template <class T>
    DescriptorFlags ES5ArrayTypeHandlerBase<T>::GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        return __super::GetSetter(instance, propertyNameString, setterValue, info, requestContext);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags flags)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            return DeleteItem(ES5Array::FromVar(instance), instance, index, flags);
        }

        return __super::DeleteProperty(instance, propertyId, flags);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::HasItem(DynamicObject* instance, uint32 index)
    {
        return HasItem(ES5Array::FromVar(instance), index);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItem(DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags)
    {
        return SetItem(ES5Array::FromVar(instance), instance, index, value, flags);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItemWithAttributes(DynamicObject* instance, uint32 index, Var value, PropertyAttributes attributes)
    {
        return SetItemWithAttributes(ES5Array::FromVar(instance), instance, index, value, attributes);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItemAttributes(DynamicObject* instance, uint32 index, PropertyAttributes attributes)
    {
        return SetItemAttributes(ES5Array::FromVar(instance), instance, index, attributes);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItemAccessors(DynamicObject* instance, uint32 index, Var getter, Var setter)
    {
        return SetItemAccessors(ES5Array::FromVar(instance), instance, index, getter, setter);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::DeleteItem(DynamicObject* instance, uint32 index, PropertyOperationFlags flags)
    {
        return DeleteItem(ES5Array::FromVar(instance), instance, index, flags);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetItem(DynamicObject* instance, Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {
        return GetItem(ES5Array::FromVar(instance), instance, originalInstance, index, value, requestContext);
    }

    template <class T>
    DescriptorFlags ES5ArrayTypeHandlerBase<T>::GetItemSetter(DynamicObject* instance, uint32 index, Var* setterValue, ScriptContext* requestContext)
    {
        return GetItemSetter(ES5Array::FromVar(instance), instance, index, setterValue, requestContext);
    }

    template <class T>
    bool ES5ArrayTypeHandlerBase<T>::IsLengthWritable() const
    {
        return lengthWritable;
    }

    template <class T>
    void ES5ArrayTypeHandlerBase<T>::SetLengthWritable(bool writable)
    {
        lengthWritable = writable;

        if (!writable)
        {
            ClearHasOnlyWritableDataProperties();
        }
    }

    //
    // Try to delete the range [firstKey, length) from right to left, stop if running into an element whose
    // [[CanDelete]] is false. Return the index where [index, ...) can all be deleted.
    //
    // Note that this helper method finds the max range to delete but may or may not delete the data items.
    // The caller needs to call JavascriptArray::SetLength to trim the data items.
    //
    template <class T>
    uint32 ES5ArrayTypeHandlerBase<T>::DeleteDownTo(ES5Array* arr, uint32 first, PropertyOperationFlags propertyOperationFlags)
    {
        Assert(first < arr->GetLength()); // Only called when newLen < oldLen

        // If the number of elements to be deleted is small, iterate on it.
        uint32 count = arr->GetLength() - first;
        if (count < 5)
        {
            uint32 oldLen = arr->GetLength();
            while (first < oldLen)
            {
                if (!arr->DeleteItem(oldLen - 1, propertyOperationFlags))
                {
                    break;
                }
                --oldLen;
            }

            return oldLen;
        }

        // If data items are [[CanDelete]], check attribute map only.
        if (GetDataItemAttributes() & PropertyConfigurable)
        {
            return indexPropertyMap->DeleteDownTo(first);
        }
        else
        {
            // The array isSealed. No existing item can be deleted. Look for the max index.
            uint32 lastIndex;
            if (indexPropertyMap->TryGetLastIndex(&lastIndex) && lastIndex >= first)
            {
                first = lastIndex + 1;
            }
            if (TryGetLastDataItemIndex(arr, first, &lastIndex))
            {
                first = lastIndex + 1;
            }
            return first;
        }
    }

    //
    // Try get the last data item index in the range of [first, length).
    //
    template <class T>
    bool ES5ArrayTypeHandlerBase<T>::TryGetLastDataItemIndex(ES5Array* arr, uint32 first, uint32* lastIndex)
    {
        uint32 index = JavascriptArray::InvalidIndex;

        JavascriptArray::ArrayElementEnumerator e(arr, first);
        while (e.MoveNext<Var>())
        {
            index = e.GetIndex();
        }

        if (index != JavascriptArray::InvalidIndex)
        {
            *lastIndex = index;
            return true;
        }

        return false;
    }

    template <class T>
    void ES5ArrayTypeHandlerBase<T>::SetLength(ES5Array* arr, uint32 newLen, PropertyOperationFlags propertyOperationFlags)
    {
        Assert(IsLengthWritable()); // Should have already checked

        if (newLen < arr->GetLength())
        {
            newLen = DeleteDownTo(arr, newLen, propertyOperationFlags); // Result newLen might be different
        }

        // Trim data items and set length
        arr->SetLength(newLen);

        //
        // Strict mode TODO: In strict mode we may need to throw if we cannot delete to
        // requested newLen (ES5 15.4.5.1 3.l.III.4).
        //
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsAttributeSet(uint32 index, PropertyAttributes attr)
    {
        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {
            if (!(descriptor->Attributes & PropertyDeleted))
            {
                return descriptor->Attributes & attr;
            }
        }
        else
        {
            return GetDataItemAttributes() & attr;
        }

        return true;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsAttributeSet(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attr, BOOL& isNumericPropertyId)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();

        uint32 index;
        isNumericPropertyId = scriptContext->IsNumericPropertyId(propertyId, &index);
        if (isNumericPropertyId)
        {
            return IsAttributeSet(index, attr);
        }

        return true;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::UpdateAttribute(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attr, BOOL value, BOOL& isNumericPropertyId)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();

        uint32 index;
        isNumericPropertyId = scriptContext->IsNumericPropertyId(propertyId, &index);
        if (isNumericPropertyId)
        {
            IndexPropertyDescriptor* descriptor;
            if (indexPropertyMap->TryGetReference(index, &descriptor))
            {
                if (descriptor->Attributes & PropertyDeleted)
                {
                    return false;
                }

                if (value)
                {
                    descriptor->Attributes |= attr;
                }
                else
                {
                    descriptor->Attributes &= (~attr);
                    if (!(descriptor->Attributes & PropertyWritable))
                    {
                        this->ClearHasOnlyWritableDataProperties();
                        if(GetFlags() & IsPrototypeFlag)
                        {
                            instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                        }
                    }
                }
            }
            else
            {
                if (!HasDataItem(ES5Array::FromVar(instance), index))
                {
                    return false;
                }

                PropertyAttributes newAttr = GetDataItemAttributes();
                if (value)
                {
                    newAttr |= attr;
                }
                else
                {
                    newAttr &= (~attr);
                }

                if (newAttr != GetDataItemAttributes())
                {
                    indexPropertyMap->Add(index, IndexPropertyDescriptor(newAttr));
                    if (!(newAttr & PropertyWritable))
                    {
                        this->ClearHasOnlyWritableDataProperties();
                        if(GetFlags() & IsPrototypeFlag)
                        {
                            instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                        }
                    }
                }
            }

            return true;
        }

        return false;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsItemEnumerable(ES5Array* arr, uint32 index)
    {
        return IsAttributeSet(index, PropertyEnumerable);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {
        BOOL isNumericPropertyId;
        return IsAttributeSet(instance, propertyId, PropertyEnumerable, isNumericPropertyId)
            && (isNumericPropertyId || __super::IsEnumerable(instance, propertyId));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {
        BOOL isNumericPropertyId;
        return IsAttributeSet(instance, propertyId, PropertyWritable, isNumericPropertyId)
            && (isNumericPropertyId || __super::IsWritable(instance, propertyId));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {
        BOOL isNumericPropertyId;
        return IsAttributeSet(instance, propertyId, PropertyConfigurable, isNumericPropertyId)
            && (isNumericPropertyId || __super::IsConfigurable(instance, propertyId));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        if (propertyId == PropertyIds::length)
        {
            Assert(!value); // Can only set enumerable to false
            return true;
        }

        BOOL isNumericPropertyId;
        return UpdateAttribute(instance, propertyId, PropertyEnumerable, value, isNumericPropertyId)
            || (!isNumericPropertyId && __super::SetEnumerable(instance, propertyId, value));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        if (propertyId == PropertyIds::length)
        {
            SetLengthWritable(value ? true : false);
            if(!value && GetFlags() & IsPrototypeFlag)
            {
                instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
            return true;
        }

        BOOL isNumericPropertyId;
        return UpdateAttribute(instance, propertyId, PropertyWritable, value, isNumericPropertyId)
            || (!isNumericPropertyId && __super::SetWritable(instance, propertyId, value));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        if (propertyId == PropertyIds::length)
        {
            Assert(!value); // Can only set configurable to false
            return true;
        }

        BOOL isNumericPropertyId;
        return UpdateAttribute(instance, propertyId, PropertyConfigurable, value, isNumericPropertyId)
            || (!isNumericPropertyId && __super::SetConfigurable(instance, propertyId, value));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetAccessors(DynamicObject* instance, PropertyId propertyId, Var* getter, Var* setter)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            return GetItemAccessors(ES5Array::FromVar(instance), instance, index, getter, setter);
        }

        return __super::GetAccessors(instance, propertyId, getter, setter);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::Seal(DynamicObject* instance)
    {
        IndexPropertyDescriptor* descriptor = NULL;
        for (int i = 0; i < indexPropertyMap->Count(); i++)
        {
            descriptor = indexPropertyMap->GetReferenceAt(i);
            descriptor->Attributes &= (~PropertyConfigurable);
        }

        this->SetDataItemSealed(); // set shared data item attributes sealed

        return __super::Seal(instance);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {
        ES5Array* arr = ES5Array::FromVar(instance);

        for (int i = 0; i < indexPropertyMap->Count(); i++)
        {
            uint32 index = indexPropertyMap->GetKeyAt(i);
            IndexPropertyDescriptor* descriptor = indexPropertyMap->GetReferenceAt(i);

            if (HasDataItem(arr, index))
            {
                //Only data descriptor has Writable property
                descriptor->Attributes &= ~(PropertyWritable | PropertyConfigurable);
            }
            else
            {
                descriptor->Attributes &= ~(PropertyConfigurable);
            }
        }

        this->SetDataItemFrozen(); // set shared data item attributes frozen
        SetLengthWritable(false); // Freeze "length" as well

        return __super::FreezeImpl(instance, isConvertedType);
    }

    template <class T>
    BigDictionaryTypeHandler* ES5ArrayTypeHandlerBase<T>::NewBigDictionaryTypeHandler(Recycler* recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots)
    {
        return RecyclerNew(recycler, BigES5ArrayTypeHandler, recycler, slotCapacity, inlineSlotCapacity, offsetOfInlineSlots, this);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsSealed(DynamicObject* instance)
    {
        if (!__super::IsSealed(instance))
        {
            return false;
        }

        for (int i = 0; i < indexPropertyMap->Count(); i++)
        {
            IndexPropertyDescriptor* descriptor = indexPropertyMap->GetReferenceAt(i);
            if (descriptor->Attributes & PropertyDeleted)
            {
                continue; // Skip deleted
            }
            if (descriptor->Attributes & PropertyConfigurable)
            {
                //[[Configurable]] must be false for all properties.
                return false;
            }
        }

        // Check data item not in map
        if (this->GetDataItemAttributes() & PropertyConfigurable)
        {
            if (HasAnyDataItemNotInMap(ES5Array::FromVar(instance)))
            {
                return false;
            }
        }

        return true;
    }

    //
    // When arr is objectArray of an object, we should skip "length" while testing isFrozen. "length" is an
    // own property of arr, but not of the containing object.
    //
    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsObjectArrayFrozen(ES5Array* arr)
    {
        if (!__super::IsFrozen(arr))
        {
            return false;
        }

        for (int i = 0; i < indexPropertyMap->Count(); i++)
        {
            uint32 index = indexPropertyMap->GetKeyAt(i);
            IndexPropertyDescriptor* descriptor = indexPropertyMap->GetReferenceAt(i);

            if (descriptor->Attributes & PropertyDeleted)
            {
                continue; // Skip deleted
            }
            if (descriptor->Attributes & PropertyConfigurable)
            {
                return false;
            }

            if ((descriptor->Attributes & PropertyWritable) && HasDataItem(arr, index))
            {
                //Only data descriptor has Writable property
                return false;
            }
        }

        // Check data item not in map
        if (this->GetDataItemAttributes() & (PropertyWritable | PropertyConfigurable))
        {
            if (HasAnyDataItemNotInMap(arr))
            {
                return false;
            }
        }

        return true;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsFrozen(DynamicObject* instance)
    {
        // We need to check "length" frozen for standalone ES5Array
        if (IsLengthWritable())
        {
            return false;
        }

        return IsObjectArrayFrozen(ES5Array::FromVar(instance));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            return SetItemAttributes(ES5Array::FromVar(instance), instance, index, attributes);
        }

        return __super::SetAttributes(instance, propertyId, attributes);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsValidDescriptorToken(void * descriptorValidationToken) const
    {
        return indexPropertyMap->IsValidDescriptorToken(descriptorValidationToken);
    }

    template <class T>
    uint32 ES5ArrayTypeHandlerBase<T>::GetNextDescriptor(uint32 key, IndexPropertyDescriptor** descriptor, void ** descriptorValidationToken)
    {
        return indexPropertyMap->GetNextDescriptor(key, descriptor, descriptorValidationToken);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetDescriptor(uint32 index, Js::IndexPropertyDescriptor **ppDescriptor) {
        return indexPropertyMap->TryGetReference(index, ppDescriptor);
    }

    template class ES5ArrayTypeHandlerBase<PropertyIndex>;
    template class ES5ArrayTypeHandlerBase<BigPropertyIndex>;
}

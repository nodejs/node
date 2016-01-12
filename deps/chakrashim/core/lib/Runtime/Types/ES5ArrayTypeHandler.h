//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class IndexPropertyDescriptor
    {
    public:
        PropertyAttributes Attributes;
        Var Getter;
        Var Setter;

        IndexPropertyDescriptor(PropertyAttributes attributes = PropertyDynamicTypeDefaults,
            Var getter = NULL, Var setter = NULL)
            : Attributes(attributes), Getter(getter), Setter(setter)
        {
        }

        IndexPropertyDescriptor(Var getter, Var setter)
            : Attributes(PropertyDynamicTypeDefaults), Getter(getter), Setter(setter)
        {
        }
    };

    //
    // IndexPropertyDescriptorMap uses a dictionary for quick index attribute look up. When visiting
    // in order is needed, it creates an ordered index list on the fly.
    //
    class IndexPropertyDescriptorMap
    {
    private:
        // Note: IndexPropertyDescriptor contains references. We need to allocate entries as non-leaf node.
        typedef JsUtil::BaseDictionary<uint32, IndexPropertyDescriptor, ForceNonLeafAllocator<Recycler>::AllocatorType, PowerOf2SizePolicy>
            InnerMap;

        Recycler* recycler;
        InnerMap* indexPropertyMap; // The internal real index property map
        uint32* indexList;          // The index list that's created on demand
        int lastIndexAt;            // Last used index list entry

    private:
        void EnsureIndexList();

    public:
        IndexPropertyDescriptorMap(Recycler* recycler);

        void Add(uint32 key, const IndexPropertyDescriptor& descriptor);
        bool TryGetLastIndex(uint32* lastIndex);
        BOOL IsValidDescriptorToken(void * descriptorValidationToken) const;
        uint32 GetNextDescriptor(uint32 key, IndexPropertyDescriptor** descriptor, void ** descriptorValidationToken);
        uint32 DeleteDownTo(uint32 firstKey);

        int Count() const
        {
            return indexPropertyMap->Count();
        }
        uint32 GetKeyAt(int i) const
        {
            return indexPropertyMap->GetKeyAt(i);
        }
        IndexPropertyDescriptor* GetReferenceAt(int i) const
        {
            return indexPropertyMap->GetReferenceAt(i);
        }
        bool ContainsKey(uint32 key) const
        {
            return indexPropertyMap->ContainsKey(key);
        }
        bool TryGetReference(uint32 key, IndexPropertyDescriptor** value) const
        {
            return indexPropertyMap->TryGetReference(key, value);
        }

    private:
        static int __cdecl CompareIndex(const void* left, const void* right)
        {
            return *static_cast<const uint32*>(left) - *static_cast<const uint32*>(right);
        }
    };

    //
    // Private type handler used by ES5Array
    //
    template <class T>
    class ES5ArrayTypeHandlerBase sealed: public DictionaryTypeHandlerBase<T>
    {
        friend class NullTypeHandlerBase;
        friend class DeferredTypeHandlerBase;
        template <DeferredTypeInitializer initializer, typename DeferredTypeFilter, bool isPrototypeTemplate, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots>
        friend class DeferredTypeHandler;
        friend class PathTypeHandlerBase;
        template<size_t size>
        friend class SimpleTypeHandler;
        template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported> friend class SimpleDictionaryTypeHandlerBase;
        template <typename T> friend class DictionaryTypeHandlerBase;
        template <typename T> friend class ES5ArrayTypeHandlerBase;

    private:
        IndexPropertyDescriptorMap* indexPropertyMap;
        PropertyAttributes dataItemAttributes; // attributes for data item not in map
        bool lengthWritable;

    public:
        DEFINE_GETCPPNAME();

    private:
        ES5ArrayTypeHandlerBase(Recycler* recycler);
        ES5ArrayTypeHandlerBase(Recycler* recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots);
        ES5ArrayTypeHandlerBase(Recycler* recycler, DictionaryTypeHandlerBase<T>* typeHandler);
        DEFINE_VTABLE_CTOR_NO_REGISTER(ES5ArrayTypeHandlerBase, DictionaryTypeHandlerBase<T>);

        // This constructor is used to grow small ES5ArrayTypeHandler into BigES5ArrayTypeHandler. We simply take over all own fields here
        // as the Small/Big difference only exists in base DictionaryTypeHandler. Base class is responsible to handle non-index properties.
        template <class SmallIndexType>
        ES5ArrayTypeHandlerBase(Recycler* recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, ES5ArrayTypeHandlerBase<SmallIndexType>* typeHandler)
            : DictionaryTypeHandlerBase<T>(recycler, slotCapacity, inlineSlotCapacity, offsetOfInlineSlots),
            indexPropertyMap(typeHandler->indexPropertyMap),
            dataItemAttributes(typeHandler->dataItemAttributes),
            lengthWritable(typeHandler->lengthWritable)
        {
        }

        void SetInstanceTypeHandler(DynamicObject * instance, bool hasChanged = true);
        BOOL HasDataItem(ES5Array* arr, uint32 index);
        bool HasAnyDataItemNotInMap(ES5Array* arr);
        PropertyAttributes GetDataItemAttributes() const;
        void SetDataItemSealed();
        void SetDataItemFrozen();

        static BOOL CantAssign(PropertyOperationFlags flags, ScriptContext* scriptContext);
        static BOOL CantExtend(PropertyOperationFlags flags, ScriptContext* scriptContext);

        BOOL IsAttributeSet(uint32 index, PropertyAttributes attr);
        BOOL IsAttributeSet(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attr, BOOL& isNumericPropertyId);
        BOOL UpdateAttribute(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attr, BOOL value, BOOL& isNumericPropertyId);

        uint32 DeleteDownTo(ES5Array* arr, uint32 first, PropertyOperationFlags propertyOperationFlags);
        bool TryGetLastDataItemIndex(ES5Array* arr, uint32 first, uint32* lastIndex);
        bool CanSetItemAt(ES5Array* arr, uint32 index) const;

    public:
        // Create a new type handler for a future DynamicObject. This is for public usage. "initialCapacity" indicates desired slotCapacity, subject to alignment round up.
        static ES5ArrayTypeHandlerBase* New(Recycler * recycler, int initialCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots);

        void SetLengthWritable(bool writable);

        BOOL HasItem(ES5Array* arr, uint32 index);
        BOOL SetItem(ES5Array* arr, DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags);
        BOOL SetItemWithAttributes(ES5Array* arr, DynamicObject* instance, uint32 index, Var value, PropertyAttributes attributes);
        BOOL SetItemAttributes(ES5Array* arr, DynamicObject* instance, uint32 index, PropertyAttributes attributes);
        BOOL SetItemAccessors(ES5Array* arr, DynamicObject* instance, uint32 index, Var getter, Var setter);
        BOOL DeleteItem(ES5Array* arr, DynamicObject* instance, uint32 index, PropertyOperationFlags propertyOperationFlags);
        BOOL GetItem(ES5Array* arr, DynamicObject* instance, Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext);
        DescriptorFlags GetItemSetter(ES5Array* arr, DynamicObject* instance, uint32 index, Var* setterValue, ScriptContext* requestContext);
        BOOL GetItemAccessors(ES5Array* arr, DynamicObject* instance, uint32 index, Var* getter, Var* setter);

    public:
        virtual BOOL HasProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl = nullptr) override;
        virtual BOOL HasProperty(DynamicObject* instance, JavascriptString* propertyNameString) override;
        virtual BOOL GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual DescriptorFlags GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual DescriptorFlags GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags flags) override;

        virtual BOOL HasItem(DynamicObject* instance, uint32 index) override;
        virtual BOOL SetItem(DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags) override;
        virtual BOOL SetItemWithAttributes(DynamicObject* instance, uint32 index, Var value, PropertyAttributes attributes) override;
        virtual BOOL SetItemAttributes(DynamicObject* instance, uint32 index, PropertyAttributes attributes) override;
        virtual BOOL SetItemAccessors(DynamicObject* instance, uint32 index, Var getter, Var setter) override;
        virtual BOOL DeleteItem(DynamicObject* instance, uint32 index, PropertyOperationFlags flags) override;
        virtual BOOL GetItem(DynamicObject* instance, Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext) override;
        virtual DescriptorFlags GetItemSetter(DynamicObject* instance, uint32 index, Var* setterValue, ScriptContext* requestContext) override;
        virtual BOOL IsEnumerable(DynamicObject* instance, PropertyId propertyId) override;
        virtual BOOL IsWritable(DynamicObject* instance, PropertyId propertyId) override;
        virtual BOOL IsConfigurable(DynamicObject* instance, PropertyId propertyId) override;
        virtual BOOL SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value) override;
        virtual BOOL SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value) override;
        virtual BOOL SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value) override;
        virtual BOOL GetAccessors(DynamicObject* instance, PropertyId propertyId, Var* getter, Var* setter) override;
        virtual BOOL Seal(DynamicObject* instance) override;
        virtual BOOL IsSealed(DynamicObject* instance) override;
        virtual BOOL IsFrozen(DynamicObject* instance) override;
        virtual BOOL SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes) override;

        virtual bool IsLengthWritable() const override;
        virtual void SetLength(ES5Array* arr, uint32 newLen, PropertyOperationFlags propertyOperationFlags) override;
        virtual BOOL IsObjectArrayFrozen(ES5Array* arr) override;
        virtual BOOL IsItemEnumerable(ES5Array* arr, uint32 index) override;
        virtual BOOL IsValidDescriptorToken(void * descriptorValidationToken) const override;
        virtual uint32 GetNextDescriptor(uint32 key, IndexPropertyDescriptor** descriptor, void ** descriptorValidationToken) override;
        virtual BOOL GetDescriptor(uint32 index, Js::IndexPropertyDescriptor **ppDescriptor) override;

        virtual void SetIsPrototype(DynamicObject* instance) override;
    private:
        virtual BOOL FreezeImpl(DynamicObject* instance, bool isConvertedType) override;
        virtual BigDictionaryTypeHandler* NewBigDictionaryTypeHandler(Recycler* recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots) override;
    };
}

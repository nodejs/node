//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//
// This type handler supports storage of a map of property Id to index along with
// property descriptors for every property.
//
// It can be shared across objects as long as no properties are
// added, deleted or changed. Once the property structure is changed the type is marked as
// non-shared. In non-shared state, any property structural changes can happen without creating
// a new type or handler.
//
// Type transition to DictionaryTypeHandler happens on the use of setters.
//

#pragma once

namespace Js
{
    template <typename TMapKey>
    struct PropertyMapKeyTraits
    {
    };

    template <>
    struct PropertyMapKeyTraits<const PropertyRecord*>
    {
        template <typename TKey, typename TValue>
        class Entry : public JsUtil::SimpleDictionaryEntry<TKey, TValue> { };

        static bool IsStringTypeHandler() { return false; };
    };

    template <>
    struct PropertyMapKeyTraits<JavascriptString*>
    {
        template <typename TKey, typename TValue>
        class Entry : public JsUtil::DictionaryEntry<TKey, TValue> { };

        static bool IsStringTypeHandler() { return true; };
    };

    // Template parameters:
    // - TPropertyIndex: property index type: PropertyIndex, BigPropertyIndex, etc.
    // - TMapKey: key type for property map: PropertyRecord* const, JavascriptString*
    // - IsNotExtensibleSupported: whether the following features are supported preventExtensions, seal, freeze.
    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
    class SimpleDictionaryTypeHandlerBase : public DynamicTypeHandler
    {
    private:
        friend class NullTypeHandlerBase;
        friend class DeferredTypeHandlerBase;
        friend class PathTypeHandlerBase;
        template<size_t size>
        friend class SimpleTypeHandler;

        template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported> friend class SimpleDictionaryTypeHandlerBase;

        // Explicit non leaf allocator now that the key is non-leaf
        typedef JsUtil::BaseDictionary<TMapKey, SimpleDictionaryPropertyDescriptor<TPropertyIndex>, RecyclerNonLeafAllocator, DictionarySizePolicy<PowerOf2Policy, 1>, PropertyRecordStringHashComparer, PropertyMapKeyTraits<TMapKey>::Entry>
            SimplePropertyDescriptorMap;
        typedef SimplePropertyDescriptorMap PropertyDescriptorMapType; // alias used by diagnostics

    protected:
        SimplePropertyDescriptorMap* propertyMap;

    private:
        RecyclerWeakReference<DynamicObject>* singletonInstance;
        TPropertyIndex nextPropertyIndex;

    protected:
        // Determines whether this instance is actually a SimpleDictionaryUnorderedTypeHandler
        bool isUnordered : 1;
        // Tracks if an InternalPropertyRecord or symbol has been added to this type; will prevent conversion to string-keyed type handler
        bool hasNamelessPropertyId : 1;

    private:
        // Number of deleted properties in the property map
        byte numDeletedProperties;

    public:
        DEFINE_GETCPPNAME();

    protected:
        SimpleDictionaryTypeHandlerBase(Recycler * recycler);
        SimpleDictionaryTypeHandlerBase(Recycler * recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false);
        SimpleDictionaryTypeHandlerBase(ScriptContext * scriptContext, SimplePropertyDescriptor const* propertyDescriptors, int propertyCount, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false);
        SimpleDictionaryTypeHandlerBase(Recycler* recycler, int slotCapacity, int propertyCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false);
        DEFINE_VTABLE_CTOR_NO_REGISTER(SimpleDictionaryTypeHandlerBase, DynamicTypeHandler);

        typedef PropertyIndexRanges<TPropertyIndex> PropertyIndexRangesType;
        static const TPropertyIndex MaxPropertyIndexSize = PropertyIndexRangesType::MaxValue;
        static const TPropertyIndex NoSlots = PropertyIndexRangesType::NoSlots;

    public:
        typedef TPropertyIndex PropertyIndexType;

        // Create a new type handler for a future DynamicObject. This is for public usage. "initialCapacity" indicates desired slotCapacity, subject to alignment round up.
        static SimpleDictionaryTypeHandlerBase * New(Recycler * recycler, int initialCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false);

        // Create a new type handler for a future DynamicObject. This is for public usage. "propertyCount" indicates desired slotCapacity, subject to alignment round up.
        static SimpleDictionaryTypeHandlerBase * New(ScriptContext * scriptContext, SimplePropertyDescriptor const* propertyDescriptors, int propertyCount, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false);

        static DynamicType* CreateTypeForNewScObject(ScriptContext* scriptContext, DynamicType* type, const Js::PropertyIdArray *propIds, bool shareType, bool check__proto__);

        virtual BOOL IsStringTypeHandler() const override { return PropertyMapKeyTraits<TMapKey>::IsStringTypeHandler(); }

        virtual BOOL IsLockable() const override { return true; }
        virtual BOOL IsSharable() const override { return true; }
        virtual void DoShareTypeHandler(ScriptContext* scriptContext) override;

        virtual int GetPropertyCount() override;

        virtual PropertyId GetPropertyId(ScriptContext* scriptContext, PropertyIndex index) override;
        virtual PropertyId GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index) override;
        virtual PropertyIndex GetPropertyIndex(const PropertyRecord* propertyRecord) override;
        virtual bool GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info) override;
        virtual bool IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex) override;
        virtual bool IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry* entry) override;

        virtual BOOL FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyString,
            PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols = false) override;
        virtual BOOL FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyString,
            PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols = false) override;

        virtual BOOL HasProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl = nullptr) override;
        virtual BOOL HasProperty(DynamicObject* instance, JavascriptString* propertyNameString) override;
        virtual BOOL GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual DescriptorFlags GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual DescriptorFlags GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags flags) override sealed;

        virtual PropertyIndex GetRootPropertyIndex(const PropertyRecord* propertyRecord) override;

        virtual BOOL HasRootProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl, bool *pDeclaredProperty = nullptr) override;
        virtual BOOL GetRootProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL SetRootProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual DescriptorFlags GetRootSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL DeleteRootProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags flags) override;

        virtual BOOL IsSimpleDictionaryTypeHandler() const {return TRUE;}
#if DBG
        virtual bool IsLetConstGlobal(DynamicObject* instance, PropertyId propertyId) override;
#endif
        virtual bool NextLetConstGlobal(int& index, RootObjectBase* instance, const PropertyRecord** propertyRecord, Var* value, bool* isConst) override;

        virtual BOOL IsFixedProperty(const DynamicObject* instance, PropertyId propertyId) override;
        virtual BOOL IsEnumerable(DynamicObject* instance, PropertyId propertyId) override;
        virtual BOOL IsWritable(DynamicObject* instance, PropertyId propertyId) override;
        virtual BOOL IsConfigurable(DynamicObject* instance, PropertyId propertyId) override;
        virtual BOOL SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value) override;
        virtual BOOL SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value) override;
        virtual BOOL SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value) override sealed;
        virtual BOOL SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags = PropertyOperation_None) override;
        virtual BOOL PreventExtensions(DynamicObject *instance) override;
        virtual BOOL Seal(DynamicObject* instance) override;
        virtual BOOL SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags = PropertyOperation_None, SideEffects possibleSideEffects = SideEffects_Any) override sealed;
        virtual BOOL IsSealed(DynamicObject* instance) override;
        virtual BOOL IsFrozen(DynamicObject* instance) override;
        virtual BOOL SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes) override sealed;
        virtual BOOL GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes) override;
        virtual BOOL SetItem(DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags) override sealed;

        virtual void SetAllPropertiesToUndefined(DynamicObject* instance, bool invalidateFixedFields) override;
        virtual void MarshalAllPropertiesToScriptContext(DynamicObject* instance, ScriptContext* targetScriptContext, bool invalidateFixedFields) override;
        virtual DynamicTypeHandler* ConvertToTypeWithItemAttributes(DynamicObject* instance) override;

        virtual void SetIsPrototype(DynamicObject* instance) override;

#if DBG
        virtual bool SupportsPrototypeInstances() const { return true; }
#endif

        virtual bool HasSingletonInstance() const override sealed;
        virtual bool TryUseFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext) override;
        virtual bool TryUseFixedAccessor(PropertyRecord const * propertyRecord, Var * pAccessor, FixedPropertyKind propertyType, bool getter, ScriptContext * requestContext) override;

#if DBG
        virtual bool CanStorePropertyValueDirectly(const DynamicObject* instance, PropertyId propertyId, bool allowLetConst) override;
        virtual bool CheckFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, ScriptContext * requestContext) override;
        virtual bool HasAnyFixedProperties() const override;
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        virtual void DumpFixedFields() const override;
        static void TraceFixedFieldsBeforeTypeHandlerChange(
            const wchar_t* oldTypeHandlerName, const wchar_t* newTypeHandlerName,
            DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore);
        static void TraceFixedFieldsAfterTypeHandlerChange(
            DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicTypeHandler* newTypeHandler,
            DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore);
        static void TraceFixedFieldsBeforeSetIsProto(
            DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore);
        static void TraceFixedFieldsAfterSetIsProto(
            DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicTypeHandler* newTypeHandler,
            DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore);
#endif

    private:
        typedef SimpleDictionaryTypeHandlerBase<BigPropertyIndex, TMapKey, false> BigSimpleDictionaryTypeHandler;

        template <bool doLock>
        bool IsObjTypeSpecEquivalentImpl(const Type* type, const EquivalentPropertyEntry *entry);
        template <bool allowNonExistent, bool markAsUsed>
        bool TryGetFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext);

    public:
        virtual RecyclerWeakReference<DynamicObject>* GetSingletonInstance() const sealed { Assert(HasSingletonInstanceOnlyIfNeeded()); return this->singletonInstance; }

        virtual void SetSingletonInstanceUnchecked(RecyclerWeakReference<DynamicObject>* instance) override
        {
            Assert(!GetIsShared());
            Assert(this->singletonInstance == nullptr);
            this->singletonInstance = instance;
        }

        virtual void ClearSingletonInstance() override sealed
        {
            Assert(HasSingletonInstanceOnlyIfNeeded());
            this->singletonInstance = nullptr;
        }

#if DBG
        bool HasSingletonInstanceOnlyIfNeeded() const
        {
            return AreSingletonInstancesNeeded() || this->singletonInstance == nullptr;
        }
#endif

    private:
        void SetIsPrototype(DynamicObject* instance, bool hasNewType);
        template <typename TPropertyKey>
        void InvalidateFixedField(const TPropertyKey propertyKey, SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor, ScriptContext* scriptContext);

        bool SupportsSwitchingToUnordered(const ScriptContext *const scriptContext) const;
        SimpleDictionaryUnorderedTypeHandler<TPropertyIndex, TMapKey, IsNotExtensibleSupported> *AsUnordered();
        void SetNumDeletedProperties(const byte n);

        template <typename U, typename UMapKey>
        U* ConvertToTypeHandler(DynamicObject* instance);

        template <typename TPropertyKey>
        void Add(TPropertyKey propertyKey, PropertyAttributes attributes, ScriptContext *const scriptContext);
        template <typename TPropertyKey>
        void Add(TPropertyKey propertyKey, PropertyAttributes attributes, bool isInitialized, bool isFixed, bool usedAsFixed, ScriptContext *const scriptContext);
        template <typename TPropertyKey>
        void Add(TPropertyIndex propertyIndex, TPropertyKey propertyKey, PropertyAttributes attributes, ScriptContext *const scriptContext);
        template <typename TPropertyKey>
        void Add(TPropertyIndex propertyIndex, TPropertyKey propertyKey, PropertyAttributes attributes, bool isIntiailized, bool isFixed, bool usedAsFixed, ScriptContext *const scriptContext);
        DictionaryTypeHandlerBase<TPropertyIndex>* ConvertToDictionaryType(DynamicObject* instance);
        ES5ArrayTypeHandlerBase<TPropertyIndex>* ConvertToES5ArrayType(DynamicObject* instance);
        SimpleDictionaryTypeHandlerBase* ConvertToNonSharedSimpleDictionaryType(DynamicObject* instance);
        template <typename NewTPropertyIndex, typename NewTMapKey, bool NewIsNotExtensibleSupported> SimpleDictionaryUnorderedTypeHandler<NewTPropertyIndex, NewTMapKey, NewIsNotExtensibleSupported>* ConvertToSimpleDictionaryUnorderedTypeHandler(DynamicObject* instance);
        BOOL SetAttribute(DynamicObject* instance, SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor, PropertyAttributes attribute);
        BOOL ClearAttribute(DynamicObject* instance, SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor, PropertyAttributes attribute);
        void EnsureSlotCapacity(DynamicObject * instance);
        template <typename TPropertyKey>
        BOOL AddProperty(DynamicObject* instance, TPropertyKey propertyKey, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects);
        virtual BOOL FreezeImpl(DynamicObject* instance, bool isConvertedType) override;

        template <bool allowLetConstGlobal>
        __inline BOOL HasProperty_Internal(DynamicObject* instance, PropertyId propertyId, bool *noRedecl, bool *pDeclaredProperty);
        template <bool allowLetConstGlobal>
        __inline PropertyIndex GetPropertyIndex_Internal(const PropertyRecord* propertyRecord);
        template <bool allowLetConstGlobal>
        __inline BOOL GetProperty_Internal(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext);
        template <bool allowLetConstGlobal>
        __inline BOOL SetProperty_Internal(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info);
        template <bool allowLetConstGlobal>
        __inline DescriptorFlags GetSetter_Internal(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext);
        template <bool allowLetConstGlobal>
        __inline BOOL DeleteProperty_Internal(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags flags);

        template <bool allowLetConstGlobal>
        __inline BOOL GetPropertyFromDescriptor(DynamicObject* instance, SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor, Var* value, PropertyValueInfo* info);
        template <bool allowLetConstGlobal, typename TPropertyKey>
        __inline BOOL SetPropertyFromDescriptor(DynamicObject* instance, PropertyId propertyId, TPropertyKey propertyKey, SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor, Var value, PropertyOperationFlags flags, PropertyValueInfo* info);
        template <bool allowLetConstGlobal>
        __inline DescriptorFlags GetSetterFromDescriptor(SimpleDictionaryPropertyDescriptor<TPropertyIndex>* descriptor);

        BOOL SetProperty_JavascriptString(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info, TemplateParameter::Box<const PropertyRecord*>);
        BOOL SetProperty_JavascriptString(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info, TemplateParameter::Box<JavascriptString*>);

        BigSimpleDictionaryTypeHandler* ConvertToBigSimpleDictionaryTypeHandler(DynamicObject* instance);
        void SetPropertyValueInfo(PropertyValueInfo* info, RecyclableObject* instance, TPropertyIndex propIndex, PropertyAttributes attributes, InlineCacheFlags flags = InlineCacheNoFlags);

        BOOL PreventExtensionsInternal(DynamicObject* instance);
        BOOL SealInternal(DynamicObject* instance);
        BOOL FreezeInternal(DynamicObject* instance, bool isConvertedType = false);

        // This was added to work around not being able to specify partial template specialization of member function.
        BOOL FindNextProperty_BigPropertyIndex(ScriptContext* scriptContext, TPropertyIndex& index, JavascriptString** propertyString,
            PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols = false);

        template <bool check__proto__>
        static DynamicType* InternalCreateTypeForNewScObject(ScriptContext* scriptContext, DynamicType* type, const Js::PropertyIdArray *propIds, bool shareType);
    };

}

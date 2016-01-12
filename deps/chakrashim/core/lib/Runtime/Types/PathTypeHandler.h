//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class PathTypeHandlerBase : public DynamicTypeHandler
    {
        friend class DynamicObject;
        friend class SimplePathTypeHandler;
        friend class PathTypeHandler;
    private:
        TypePath* typePath;
        DynamicType* predecessorType; // Strong reference to predecessor type so that predecessor types remain in the cache even though they might not be used

    public:
        DEFINE_GETCPPNAME();

    protected:
        PathTypeHandlerBase(TypePath* typePath, uint16 pathLength, const PropertyIndex slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false, DynamicType* predecessorType = nullptr);

        DEFINE_VTABLE_CTOR_NO_REGISTER(PathTypeHandlerBase, DynamicTypeHandler, typePath(0));

    public:
        virtual BOOL IsLockable() const override { return true; }
        virtual BOOL IsSharable() const override { return true; }
        virtual void DoShareTypeHandler(ScriptContext* scriptContext) override;

        static bool UsePathTypeHandlerForObjectLiteral(const PropertyIdArray *const propIds, ScriptContext *const scriptContext, bool *const check__proto__Ref = nullptr);
        static DynamicType* CreateTypeForNewScObject(ScriptContext* scriptContext, DynamicType* type, const Js::PropertyIdArray *propIds, bool shareType);
        static DynamicType* CreateNewScopeObject(ScriptContext* scriptContext, DynamicType* type, const Js::PropertyIdArray *propIds, PropertyAttributes extraAttributes = PropertyNone, uint extraAttributesSlotCount = UINT_MAX);

        static PathTypeHandlerBase * FromTypeHandler(DynamicTypeHandler * const typeHandler) { Assert(typeHandler->IsPathTypeHandler()); return static_cast<PathTypeHandlerBase*>(typeHandler); }

        virtual int GetPropertyCount() override;
        virtual PropertyId GetPropertyId(ScriptContext* scriptContext, PropertyIndex index) override;
        virtual PropertyId GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index) override;
        virtual PropertyIndex GetPropertyIndex(const PropertyRecord* propertyRecord) override;
        virtual bool GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info) override;
        virtual bool IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex) override;
        virtual bool IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry* entry) override;
        virtual BOOL FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyString,
            PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols = false) override;
        virtual BOOL HasProperty(DynamicObject* instance, PropertyId propertyId, __out_opt bool *noRedecl = nullptr) override;
        virtual BOOL HasProperty(DynamicObject* instance, JavascriptString* propertyNameString) override;
        virtual BOOL GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags flags) override;
        virtual BOOL IsFixedProperty(const DynamicObject* instance, PropertyId propertyId) override;
        virtual BOOL IsEnumerable(DynamicObject* instance, PropertyId propertyId) override;
        virtual BOOL IsWritable(DynamicObject* instance, PropertyId propertyId) override;
        virtual BOOL IsConfigurable(DynamicObject* instance, PropertyId propertyId) override;
        virtual BOOL SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value) override;
        virtual BOOL SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value) override;
        virtual BOOL SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value) override;
        virtual BOOL SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags = PropertyOperation_None) override;
        virtual BOOL PreventExtensions(DynamicObject *instance) override;
        virtual BOOL Seal(DynamicObject* instance) override;
        virtual BOOL SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags = PropertyOperation_None, SideEffects possibleSideEffects = SideEffects_Any) override;
        virtual BOOL SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes) override;
        virtual BOOL GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes) override;

        virtual void ResetTypeHandler(DynamicObject * instance) override;
        virtual void SetAllPropertiesToUndefined(DynamicObject* instance, bool invalidateFixedFields) override;
        virtual void MarshalAllPropertiesToScriptContext(DynamicObject* instance, ScriptContext* targetScriptContext, bool invalidateFixedFields) override;
        virtual DynamicTypeHandler* ConvertToTypeWithItemAttributes(DynamicObject* instance) override;
        virtual BOOL AllPropertiesAreEnumerable() sealed { return true; }
        virtual BOOL IsPathTypeHandler() const { return TRUE; }

        virtual void ShrinkSlotAndInlineSlotCapacity() override;
        virtual void LockInlineSlotCapacity() override { Assert(false); };
        virtual void EnsureInlineSlotCapacityIsLocked() override;
        virtual void VerifyInlineSlotCapacityIsLocked() override;
        virtual void EnsureInlineSlotCapacityIsLocked(bool startFromRoot) = 0;
        virtual void VerifyInlineSlotCapacityIsLocked(bool startFromRoot) = 0;
        SimplePathTypeHandler *DeoptimizeObjectHeaderInlining(JavascriptLibrary *const library);
        virtual void SetPrototype(DynamicObject* instance, RecyclableObject* newPrototype) override;

        virtual void SetIsPrototype(DynamicObject* instance) override;

#if DBG
        virtual bool SupportsPrototypeInstances() const { return !IsolatePrototypes(); }
#endif

        virtual bool HasSingletonInstance() const override sealed;
        virtual bool TryUseFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext) override;
        virtual bool TryUseFixedAccessor(PropertyRecord const * propertyRecord, Var * pAccessor, FixedPropertyKind propertyType, bool getter, ScriptContext * requestContext) override;

#if DBG
        virtual bool CanStorePropertyValueDirectly(const DynamicObject* instance, PropertyId propertyId, bool allowLetConst) override;
        bool HasOnlyInitializedNonFixedProperties();
        virtual bool CheckFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, ScriptContext * requestContext) override;
        virtual bool HasAnyFixedProperties() const override;
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        virtual void DumpFixedFields() const override;
        static void TraceFixedFieldsBeforeTypeHandlerChange(
            const wchar_t* conversionName, const wchar_t* oldTypeHandlerName, const wchar_t* newTypeHandlerName,
            DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore);
        static void TraceFixedFieldsAfterTypeHandlerChange(
            DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicTypeHandler* newTypeHandler,
            DynamicType* oldType, DynamicType* newType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore);
        static void TraceFixedFieldsBeforeSetIsProto(
            DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore);
        static void TraceFixedFieldsAfterSetIsProto(
            DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicTypeHandler* newTypeHandler,
            DynamicType* oldType, DynamicType* newType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore);
#endif

    private:
        static bool FixPropsOnPathTypes()
        {
#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
            return CONFIG_FLAG(FixPropsOnPathTypes);
#else
            return false;
#endif
        }

        template <bool allowNonExistent, bool markAsUsed>
        bool TryGetFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, Js::FixedPropertyKind propertyType, ScriptContext * requestContext);

    public:
        virtual RecyclerWeakReference<DynamicObject>* GetSingletonInstance() const override sealed { return HasSingletonInstance() ? this->typePath->GetSingletonInstance() : nullptr; }

        virtual void SetSingletonInstanceUnchecked(RecyclerWeakReference<DynamicObject>* instance) override
        {
            Assert(!GetIsShared());
            this->typePath->SetSingletonInstance(instance, GetPathLength());
        }

        virtual void ClearSingletonInstance() { Assert(false); }

#if DBG
        bool HasSingletonInstanceOnlyIfNeeded() const
        {
#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
            return AreSingletonInstancesNeeded() || !this->typePath->HasSingletonInstance();
#else
            return true;
#endif
        }
#endif

    private:
        template <bool invalidateFixedFields> void DoShareTypeHandlerInternal(ScriptContext* scriptContext);

        void CopySingletonInstance(DynamicObject* instance, DynamicTypeHandler* typeHandler);
        void InvalidateFixedFieldAt(Js::PropertyId propertyId, Js::PropertyIndex index, ScriptContext* scriptContext);
        void AddBlankFieldAt(Js::PropertyId propertyId, Js::PropertyIndex index, ScriptContext* scriptContext);
        bool ProcessFixedFieldChange(DynamicObject* instance, PropertyId propertyId, PropertyIndex slotIndex, Var value, bool isNonFixed, const PropertyRecord * propertyRecord = nullptr);

    private:
        template <typename T>
        T* ConvertToTypeHandler(DynamicObject* instance);

        DynamicType* PromoteType(DynamicObject* instance, const PropertyRecord* propertyId, PropertyIndex* propertyIndex);

        DictionaryTypeHandler* ConvertToDictionaryType(DynamicObject* instance);
        ES5ArrayTypeHandler* ConvertToES5ArrayType(DynamicObject* instance);

        template <typename T> T*
        ConvertToSimpleDictionaryType(DynamicObject* instance, int propertyCapacity, bool mayBecomeShared = false);

        SimpleDictionaryTypeHandler* ConvertToSimpleDictionaryType(DynamicObject* instance, int propertyCapacity, bool mayBecomeShared = false)
        {
            return ConvertToSimpleDictionaryType<SimpleDictionaryTypeHandler>(instance, propertyCapacity, mayBecomeShared);
        }

        BOOL AddPropertyInternal(DynamicObject * instance, PropertyId propertyId, Js::Var value, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects);
        BOOL AddProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects);
        BOOL SetPropertyInternal(DynamicObject* instance, PropertyId propertyId, Var value, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects);
        virtual BOOL FreezeImpl(DynamicObject* instance, bool isConvertedType) override;

        // Checks whether conversion to shared type is needed and performs it, then calls actual operation on the shared type.
        // Template method used for PreventExtensions, Seal, Freeze.
        // FType is functor/lambda to perform actual forced operation (such as PreventExtensionsInternal) on the shared type.
        template<typename FType>
        BOOL ConvertToSharedNonExtensibleTypeIfNeededAndCallOperation(DynamicObject* instance, const PropertyRecord* operationInternalPropertyId, FType operation);

        template <bool isObjectLiteral>
        DynamicType* PromoteType(DynamicType* type, const PropertyRecord* propertyId, bool shareType, ScriptContext* scriptContext, DynamicObject* object = nullptr, PropertyIndex* propertyIndex = nullptr);

        PropertyIndex GetPropertyIndex(PropertyId propertyId);

    protected:
        virtual bool GetSuccessor(const PropertyRecord* propertyRecord, RecyclerWeakReference<DynamicType> ** typeWeakRef) = 0;
        virtual void SetSuccessor(DynamicType * type, const PropertyRecord* propertyRecord, RecyclerWeakReference<DynamicType> * typeWeakRef, ScriptContext * scriptContext) = 0;

        uint16 GetPathLength() const { return GetUnusedBytesValue(); }
        TypePath * GetTypePath() const { return typePath; }
        DynamicType * GetPredecessorType() const { return predecessorType; }
        PathTypeHandlerBase* GetRootPathTypeHandler();

    public:
        virtual void ShrinkSlotAndInlineSlotCapacity(uint16 newInlineSlotCapacity) = 0;
        virtual bool GetMaxPathLength(uint16 * maxPathLength) = 0;
    };

    typedef SimpleDictionaryTypeHandlerBase<PropertyIndex, const PropertyRecord*, true> SimpleDictionaryTypeHandlerWithNontExtensibleSupport;

    class SimplePathTypeHandler sealed : public PathTypeHandlerBase
    {
    private:
        const PropertyRecord * successorPropertyRecord;
        RecyclerWeakReference<DynamicType> * successorTypeWeakRef;

    public:
        DEFINE_GETCPPNAME();

    private:
        SimplePathTypeHandler(TypePath* typePath, uint16 pathLength, const PropertyIndex slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false, DynamicType* predecessorType = nullptr);

        DEFINE_VTABLE_CTOR_NO_REGISTER(SimplePathTypeHandler, PathTypeHandlerBase, successorPropertyRecord(nullptr), successorTypeWeakRef(nullptr));

    protected:
        virtual bool GetSuccessor(const PropertyRecord* propertyRecord, RecyclerWeakReference<DynamicType> ** typeWeakRef) override;
        virtual void SetSuccessor(DynamicType * type, const PropertyRecord* propertyRecord, RecyclerWeakReference<DynamicType> * typeWeakRef, ScriptContext * scriptContext) override;

    public:
        virtual void ShrinkSlotAndInlineSlotCapacity(uint16 newInlineSlotCapacity) override;
        virtual void LockInlineSlotCapacity() override;
        virtual bool GetMaxPathLength(uint16 * maxPathLength) override;
        virtual void EnsureInlineSlotCapacityIsLocked(bool startFromRoot) override;
        virtual void VerifyInlineSlotCapacityIsLocked(bool startFromRoot) override;

        static SimplePathTypeHandler * New(ScriptContext * scriptContext, TypePath* typePath, uint16 pathLength, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false, DynamicType* predecessorType = nullptr);
        static SimplePathTypeHandler * New(ScriptContext * scriptContext, TypePath* typePath, uint16 pathLength, const PropertyIndex slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false, DynamicType* predecessorType = nullptr);
        static SimplePathTypeHandler * SimplePathTypeHandler::New(ScriptContext * scriptContext, SimplePathTypeHandler * typeHandler, bool isLocked, bool isShared);
    };

    class PathTypeHandler sealed : public PathTypeHandlerBase
    {
        friend class SimplePathTypeHandler;

    private:
        typedef JsUtil::WeakReferenceDictionary<PropertyId, DynamicType, DictionarySizePolicy<PowerOf2Policy, 1>> PropertySuccessorsMap;
        PropertySuccessorsMap * propertySuccessors;

    public:
        DEFINE_GETCPPNAME();

    private:
        PathTypeHandler(TypePath* typePath, uint16 pathLength, const PropertyIndex slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false, DynamicType* predecessorType = nullptr);

        DEFINE_VTABLE_CTOR_NO_REGISTER(PathTypeHandler, PathTypeHandlerBase, propertySuccessors(nullptr));

    protected:
        virtual bool GetSuccessor(const PropertyRecord* propertyRecord, RecyclerWeakReference<DynamicType> ** typeWeakRef) override;
        virtual void SetSuccessor(DynamicType * type, const PropertyRecord* propertyRecord, RecyclerWeakReference<DynamicType> * typeWeakRef, ScriptContext * scriptContext) override;

    public:
        virtual void ShrinkSlotAndInlineSlotCapacity(uint16 newInlineSlotCapacity) override;
        virtual void LockInlineSlotCapacity() override;
        virtual bool GetMaxPathLength(uint16 * maxPathLength) override;
        virtual void EnsureInlineSlotCapacityIsLocked(bool startFromRoot) override;
        virtual void VerifyInlineSlotCapacityIsLocked(bool startFromRoot) override;

        static PathTypeHandler * New(ScriptContext * scriptContext, TypePath* typePath, uint16 pathLength, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false, DynamicType* predecessorType = nullptr);
        static PathTypeHandler * New(ScriptContext * scriptContext, TypePath* typePath, uint16 pathLength, const PropertyIndex slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked = false, bool isShared = false, DynamicType* predecessorType = nullptr);
        static PathTypeHandler * New(ScriptContext * scriptContext, PathTypeHandler * typeHandler, bool isLocked, bool isShared);
    };
}

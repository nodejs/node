//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    enum DeferredInitializeMode
    {
        DeferredInitializeMode_Default,
        DeferredInitializeMode_Extensions,
        DeferredInitializeMode_Set,
        DeferredInitializeMode_SetAccessors
    };

    enum FixedPropertyKind : CHAR
    {
        FixedDataProperty = 1 << 0,
        FixedMethodProperty = 1 << 1,
        FixedAccessorProperty = 1 << 2,
    };

    struct PropertyEquivalenceInfo
    {
        PropertyIndex slotIndex;
        bool isAuxSlot;
        bool isWritable;

        PropertyEquivalenceInfo():
            slotIndex(Constants::NoSlot), isAuxSlot(false), isWritable(false) {}
        PropertyEquivalenceInfo(PropertyIndex slotIndex, bool isAuxSlot, bool isWritable):
            slotIndex(slotIndex), isAuxSlot(isAuxSlot), isWritable(isWritable) {}
    };

    struct EquivalentPropertyEntry
    {
        Js::PropertyId propertyId;
        Js::PropertyIndex slotIndex;
        bool isAuxSlot;
        bool mustBeWritable;
    };

    struct TypeEquivalenceRecord
    {
        uint propertyCount;
        EquivalentPropertyEntry* properties;
    };

    typedef void (__cdecl *DeferredTypeInitializer)(DynamicObject* instance, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);

    class DynamicTypeHandler
    {
        friend class DeferredTypeHandlerBase;
        template <DeferredTypeInitializer initializer, typename DeferredTypeFilter, bool isPrototypeTemplate, uint16 _inlineSlotCapacity, uint16 _offsetOfInlineSlots>
        friend class DeferredTypeHandler;
        friend class PathTypeHandlerBase;
        friend struct InlineCache;
        friend class DynamicObject;

    private:
        // Holds flags that represent general information about the types of properties
        // handled by this handler.
        // * PropertyTypesWritableDataOnly - when true, the type being handled is known to have only writable data properties
        // * PropertyTypesWritableDataOnlyDetection - set each time SetHasOnlyWritableDataProperties is called.

        // PropertyTypesReserved (0x1) is always on so that the DWORD formed with the following boolean doesn't look like
        // a pointer.
        PropertyTypes propertyTypes;
        BYTE flags;
        uint16 offsetOfInlineSlots;
        int slotCapacity;
        uint16 unusedBytes;             // This always has it's lowest bit set to avoid false references
        uint16 inlineSlotCapacity;
        bool isNotPathTypeHandlerOrHasUserDefinedCtor;

    public:
        DEFINE_GETCPPNAME_ABSTRACT();
        DynamicTypeHandler(DynamicTypeHandler * typeHandler) :
            flags(typeHandler->flags),
            propertyTypes(typeHandler->propertyTypes),
            slotCapacity(typeHandler->slotCapacity),
            offsetOfInlineSlots(typeHandler->offsetOfInlineSlots),
            isNotPathTypeHandlerOrHasUserDefinedCtor(typeHandler->isNotPathTypeHandlerOrHasUserDefinedCtor),
            unusedBytes(typeHandler->unusedBytes)
        {
        }

    public:
        DEFINE_VTABLE_CTOR_NOBASE_ABSTRACT(DynamicTypeHandler);

        DynamicTypeHandler(int slotCapacity, uint16 inlineSlotCapacity = 0, uint16 offsetOfInlineSlots = 0, BYTE flags = DefaultFlags);

        void SetInstanceTypeHandler(DynamicObject * instance, bool hasChanged = true);

        static DynamicTypeHandler * GetCurrentTypeHandler(DynamicObject * instance);
        static void SetInstanceTypeHandler(DynamicObject * instance, DynamicTypeHandler * typeHandler, bool hasChanged = true);
        static void ReplaceInstanceType(DynamicObject * instance, DynamicType * type);

    private:
        static bool IsObjectHeaderInlined(const uint16 offsetOfInlineSlots);
        bool IsObjectHeaderInlinedTypeHandlerUnchecked() const;
    public:
        bool IsObjectHeaderInlinedTypeHandler() const;
    private:
        void VerifyObjectHeaderInlinedTypeHandler() const;
    public:
        static uint16 GetOffsetOfObjectHeaderInlineSlots();
        static PropertyIndex GetObjectHeaderInlinableSlotCapacity();

        // UnusedBytes is a tagged value to prevent GC false references
        uint16 GetUnusedBytesValue() const
        {
            return unusedBytes >> 1;
        }

        void SetUnusedBytesValue(uint16 value)
        {
            // Tagging to prevent a GC false reference
            this->unusedBytes = ((value << 1) | 1);
        }

    public:
        static const BYTE IsExtensibleFlag = 0x01;
        static const BYTE HasKnownSlot0Flag = 0x02;
        static const BYTE IsLockedFlag = 0x04;
        static const BYTE MayBecomeSharedFlag = 0x08;
        static const BYTE IsSharedFlag = 0x10;
        static const BYTE IsPrototypeFlag = 0x20;
        static const BYTE IsSealedOnceFlag = 0x40;  // Set state means the object is sealed, clear state means nothing (can be sealed, or not).
        static const BYTE IsFrozenOnceFlag = 0x80;  // Set state means the object is frozen, clear state means nothing (can be frozen, or not).
        static const BYTE DefaultFlags = IsExtensibleFlag;

    public:
        static PropertyIndex RoundUpObjectHeaderInlinedInlineSlotCapacity(const PropertyIndex slotCapacity);
        static PropertyIndex RoundUpInlineSlotCapacity(const PropertyIndex slotCapacity);
    private:
        static int RoundUpAuxSlotCapacity(const int slotCapacity);
    public:
        static int RoundUpSlotCapacity(const int slotCapacity, const PropertyIndex inlineSlotCapacity);

        uint16 GetInlineSlotCapacity() const { return this->inlineSlotCapacity; }
        int GetSlotCapacity() const { return this->slotCapacity; }

        size_t GetInlineSlotsSize() const
        {
            PropertyIndex inlineSlotsToAllocate = GetInlineSlotCapacity();
            if(IsObjectHeaderInlinedTypeHandler())
            {
                inlineSlotsToAllocate -= GetObjectHeaderInlinableSlotCapacity();
            }
            return inlineSlotsToAllocate * sizeof(Var);
        }

        uint16 GetOffsetOfInlineSlots() const { return this->offsetOfInlineSlots; }

        void EnsureSlots(DynamicObject * instance, int oldCount, int newCount, ScriptContext * scriptContext, DynamicTypeHandler * newTypeHandler = nullptr);

        Var GetSlot(DynamicObject * instance, int index);
        Var GetInlineSlot(DynamicObject * instance, int index);
        Var GetAuxSlot(DynamicObject * instance, int index);

        void TraceUseFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, bool result, LPCWSTR typeHandlerName, ScriptContext * requestContext);

        bool IsFixedMethodProperty(FixedPropertyKind fixedPropKind);
        bool IsFixedDataProperty(FixedPropertyKind fixedPropKind);
        bool IsFixedAccessorProperty(FixedPropertyKind fixedPropKind);

        static bool CheckHeuristicsForFixedDataProps(DynamicObject* instance, const PropertyRecord * propertyRecord, PropertyId propertyId, Var value);
        static bool CheckHeuristicsForFixedDataProps(DynamicObject* instance, const PropertyRecord * propertyRecord, Var value);
        static bool CheckHeuristicsForFixedDataProps(DynamicObject* instance, PropertyId propertyId, Var value);
        static bool CheckHeuristicsForFixedDataProps(DynamicObject* instance, JavascriptString * propertyKey, Var value);

#if DBG
        void SetSlot(DynamicObject * instance, PropertyId propertyId, bool allowLetConst, int index, Var value);
        void SetInlineSlot(DynamicObject * instance, PropertyId propertyId, bool allowLetConst, int index, Var value);
        void SetAuxSlot(DynamicObject * instance, PropertyId propertyId, bool allowLetConst, int index, Var value);
#else
        void SetSlot(DynamicObject * instance, int index, Var value);
        void SetInlineSlot(DynamicObject * instance, int index, Var value);
        void SetAuxSlot(DynamicObject * instance, int index, Var value);
#endif

    protected:
        void SetSlotUnchecked(DynamicObject * instance, int index, Var value);

    public:
        __inline PropertyIndex AdjustSlotIndexForInlineSlots(PropertyIndex slotIndex)
        {
            return slotIndex != Constants::NoSlot ? AdjustValidSlotIndexForInlineSlots(slotIndex) : Constants::NoSlot;
        }

        __inline PropertyIndex AdjustValidSlotIndexForInlineSlots(PropertyIndex slotIndex)
        {
            Assert(slotIndex != Constants::NoSlot);
            return slotIndex < inlineSlotCapacity ?
                slotIndex + (offsetOfInlineSlots / sizeof(Var)) : slotIndex - (PropertyIndex)inlineSlotCapacity;
        }

        __inline void PropertyIndexToInlineOrAuxSlotIndex(PropertyIndex propertyIndex, PropertyIndex * inlineOrAuxSlotIndex, bool * isInlineSlot) const
        {
            if (propertyIndex < inlineSlotCapacity)
            {
                *inlineOrAuxSlotIndex = propertyIndex + (offsetOfInlineSlots / sizeof(Var));
                *isInlineSlot = true;
            }
            else
            {
                *inlineOrAuxSlotIndex = propertyIndex - (PropertyIndex)inlineSlotCapacity;
                *isInlineSlot = false;
            }
        }

        PropertyIndex InlineOrAuxSlotIndexToPropertyIndex(PropertyIndex inlineOrAuxSlotIndex, bool isInlineSlot) const
        {
            if (isInlineSlot)
            {
                return inlineOrAuxSlotIndex - (offsetOfInlineSlots / sizeof(Var));
            }
            else
            {
                return inlineOrAuxSlotIndex + (PropertyIndex)inlineSlotCapacity;
            }
        }

    protected:
        void SetFlags(BYTE values)
        {
            // Don't set a shared flag if the type handler isn't locked.
            Assert((this->flags & IsLockedFlag) != 0 || (values & IsLockedFlag) != 0 || (values & IsSharedFlag) == 0);

            // Don't set a shared flag if the type handler isn't expecting to become shared.
            Assert((this->flags & MayBecomeSharedFlag) != 0 || (values & MayBecomeSharedFlag) != 0 || (values & IsSharedFlag) == 0);

            // It's ok to set up a shared prototype type handler through a constructor (see NullTypeHandler and DeferredTypeHandler),
            // but it's not ok to change these after the fact.

            // If we isolate prototypes, don't set a prototype flag on a type handler that is shared or may become shared
            Assert((this->flags & IsPrototypeFlag) != 0 || !IsolatePrototypes() || (this->flags & (MayBecomeSharedFlag | IsSharedFlag)) == 0 || (values & IsPrototypeFlag) == 0);
            // If we isolate prototypes, don't set a shared or may become shared flag on a prototype type handler.
            Assert((this->flags & IsSharedFlag) != 0 || !IsolatePrototypes() || (this->flags & IsPrototypeFlag) == 0 || (values & (MayBecomeSharedFlag | IsSharedFlag)) == 0);

            // Don't set a shared flag if this type handler has a singleton instance.
            Assert(!this->HasSingletonInstance() || (values & IsSharedFlag) == 0);

            this->flags |= values;
        }

        void ClearFlags(BYTE values)
        {
            // Don't clear the locked, shared or prototype flags.
            Assert((values & IsLockedFlag) == 0 && (values & IsSharedFlag) == 0 && (values & IsPrototypeFlag) == 0);

            this->flags &= ~values;
        }

        void SetFlags(BYTE selector, BYTE values)
        {
            SetFlags(selector & values);
        }

        void ChangeFlags(BYTE selector, BYTE values)
        {
            // Don't clear the locked, shared or prototype flags.
            Assert((this->flags & IsLockedFlag) == 0 || (selector & IsLockedFlag) == 0 || (values & IsLockedFlag) != 0);
            Assert((this->flags & IsSharedFlag) == 0 || (selector & IsSharedFlag) == 0 || (values & IsSharedFlag) != 0);
            Assert((this->flags & IsPrototypeFlag) == 0 || (selector & IsPrototypeFlag) == 0 || (values & IsPrototypeFlag) != 0);

            // Don't set a shared flag if the type handler isn't locked.
            Assert((this->flags & IsLockedFlag) != 0 || ((selector & values) & IsLockedFlag) != 0 || ((selector & values) & IsSharedFlag) == 0);

            // Don't set a shared flag if the type handler isn't locked.
            Assert((this->flags & MayBecomeSharedFlag) != 0 || (values & MayBecomeSharedFlag) != 0 || (values & IsSharedFlag) == 0);

            // It's ok to set up a shared prototype type handler through a constructor (see NullTypeHandler and DeferredTypeHandler),
            // but it's not ok to change these after the fact.

            // If we isolate prototypes, don't set a prototype flag on a shared type handler.
            Assert((this->flags & IsPrototypeFlag) != 0 || !IsolatePrototypes() || (this->flags & (MayBecomeSharedFlag | IsSharedFlag)) == 0 || ((selector & values) & IsPrototypeFlag) == 0);
            // If we isolate prototypes, don't set a shared flag on a prototype type handler.
            Assert((this->flags & IsSharedFlag) != 0 || !IsolatePrototypes() || (this->flags & IsPrototypeFlag) == 0 || ((selector & values) & (MayBecomeSharedFlag | IsSharedFlag)) == 0);

            // Don't set a shared flag if this type handler has a singleton instance.
            Assert(!this->HasSingletonInstance() || ((selector & values) & IsSharedFlag) == 0);

            this->flags = (selector & values) | (~selector & this->flags);
        }

        void SetPropertyTypes(PropertyTypes selector, PropertyTypes values)
        {
            Assert((selector & PropertyTypesReserved) == 0);
            Assert((this->propertyTypes & PropertyTypesReserved) != 0);
            this->propertyTypes |= (selector & values);
            Assert((this->propertyTypes & PropertyTypesReserved) != 0);
        }

        void ClearPropertyTypes(PropertyTypes selector, PropertyTypes values)
        {
            Assert((selector & PropertyTypesReserved) == 0);
            Assert((this->propertyTypes & PropertyTypesReserved) != 0);
            this->propertyTypes |= (selector & ~values);
            Assert((this->propertyTypes & PropertyTypesReserved) != 0);
        }

        void CopyPropertyTypes(PropertyTypes selector, PropertyTypes values)
        {
            Assert((selector & PropertyTypesReserved) == 0);
            Assert((this->propertyTypes & PropertyTypesReserved) != 0);
            this->propertyTypes = (selector & values) | (~selector & this->propertyTypes);
            Assert((this->propertyTypes & PropertyTypesReserved) != 0);
        }

        void CopyClearedPropertyTypes(PropertyTypes selector, PropertyTypes values)
        {
            Assert((selector & PropertyTypesReserved) == 0);
            Assert((this->propertyTypes & PropertyTypesReserved) != 0);
            this->propertyTypes = (selector & (values & this->propertyTypes)) | (~selector & this->propertyTypes);
            Assert((this->propertyTypes & PropertyTypesReserved) != 0);
        }

        static bool CanBeSingletonInstance(DynamicObject * instance);
    public:
        BYTE GetFlags() const { return this->flags; }
        static int GetOffsetOfFlags() { return offsetof(DynamicTypeHandler, flags); }
        static int GetOffsetOfOffsetOfInlineSlots() { return offsetof(DynamicTypeHandler, offsetOfInlineSlots); }

        bool GetIsLocked() const { return (this->flags & IsLockedFlag) != 0; }
        bool GetIsShared() const { return (this->flags & IsSharedFlag) != 0; }
        bool GetMayBecomeShared() const { return (this->flags & MayBecomeSharedFlag) != 0; }
        bool GetIsOrMayBecomeShared() const { return (this->flags & (MayBecomeSharedFlag | IsSharedFlag)) != 0; }
        bool GetHasKnownSlot0() const { return (this->flags & HasKnownSlot0Flag) != 0; }
        bool GetIsPrototype() const { return (this->flags & IsPrototypeFlag) != 0; }
        bool GetIsInlineSlotCapacityLocked() const { return (this->propertyTypes & PropertyTypesInlineSlotCapacityLocked) != 0; }

        void LockTypeHandler() { Assert(IsLockable()); SetFlags(IsLockedFlag); }

        void ShareTypeHandler(ScriptContext* scriptContext)
        {
            Assert(IsSharable());
            Assert(GetMayBecomeShared());
            LockTypeHandler();
            if ((GetFlags() & IsSharedFlag) == 0)
            {
                DoShareTypeHandler(scriptContext);
            }
            SetFlags(IsSharedFlag);
        }

        void SetMayBecomeShared()
        {
            SetFlags(MayBecomeSharedFlag);
        }

        void SetHasKnownSlot0()
        {
            SetFlags(HasKnownSlot0Flag);
        }

        void SetIsInlineSlotCapacityLocked()
        {
            Assert(!GetIsInlineSlotCapacityLocked());
            SetPropertyTypes(PropertyTypesInlineSlotCapacityLocked, PropertyTypesInlineSlotCapacityLocked);
        }

        PropertyTypes GetPropertyTypes() { Assert((propertyTypes & PropertyTypesReserved) != 0); return propertyTypes; }
        bool GetHasOnlyWritableDataProperties() { return (GetPropertyTypes() & PropertyTypesWritableDataOnly) == PropertyTypesWritableDataOnly; }
        // Do not use this method.  It's here only for the __proto__ performance workaround.
        void SetHasOnlyWritableDataProperties() { SetHasOnlyWritableDataProperties(true); }
        void ClearHasOnlyWritableDataProperties() { SetHasOnlyWritableDataProperties(false); };

    private:
        void SetHasOnlyWritableDataProperties(bool value)
        {
            if (value != GetHasOnlyWritableDataProperties())
            {
                propertyTypes ^= PropertyTypesWritableDataOnly;
            }

            // Turn on the detection bit.
            propertyTypes |= PropertyTypesWritableDataOnlyDetection;
            Assert((propertyTypes & PropertyTypesReserved) != 0);
        }

    public:
        void ClearWritableDataOnlyDetectionBit() { Assert((propertyTypes & PropertyTypesReserved) != 0); propertyTypes &= ~PropertyTypesWritableDataOnlyDetection; }
        bool IsWritableDataOnlyDetectionBitSet()
        {
            return (GetPropertyTypes() & PropertyTypesWritableDataOnlyDetection) == PropertyTypesWritableDataOnlyDetection;
        }

        BOOL Freeze(DynamicObject *instance, bool isConvertedType = false)  { return FreezeImpl(instance, isConvertedType); }
        bool GetIsNotPathTypeHandlerOrHasUserDefinedCtor() const { return this->isNotPathTypeHandlerOrHasUserDefinedCtor; }

        virtual BOOL IsStringTypeHandler() const { return false; }

        virtual BOOL AllPropertiesAreEnumerable() { return false; }
        virtual BOOL IsLockable() const = 0;
        virtual BOOL IsSharable() const = 0;
        virtual void DoShareTypeHandler(ScriptContext* scriptContext) {};

        virtual int GetPropertyCount() = 0;
        virtual PropertyId GetPropertyId(ScriptContext* scriptContext, PropertyIndex index) = 0;
        virtual PropertyId GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index) = 0;
        virtual BOOL FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyString,
            PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols = false) = 0;
        virtual BOOL FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyString,
            PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols = false);
        virtual PropertyIndex GetPropertyIndex(PropertyRecord const* propertyRecord) = 0;
        virtual bool GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info) = 0;
        virtual bool IsObjTypeSpecEquivalent(const Type* type, const Js::TypeEquivalenceRecord& record, uint& failedPropertyIndex) = 0;
        virtual bool IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry* entry) = 0;

        virtual bool EnsureObjectReady(DynamicObject* instance) { return true; }
        virtual BOOL HasProperty(DynamicObject* instance, PropertyId propertyId, __out_opt bool *pNoRedecl = nullptr) = 0;
        virtual BOOL HasProperty(DynamicObject* instance, JavascriptString* propertyNameString) = 0;
        virtual BOOL GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) = 0;
        virtual BOOL GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) = 0;
        virtual BOOL GetInternalProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value);
        virtual BOOL InitProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info);
        virtual BOOL SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) = 0;
        virtual BOOL SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) = 0;
        virtual BOOL SetInternalProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags);
        virtual DescriptorFlags GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext);
        virtual DescriptorFlags GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext);
        virtual BOOL DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags flags) = 0;

        // ===================================================================================================================
        // Special versions of the various *Property methods that recognize PropertyLetConstGlobal properties.
        // Only used for GlobalObject and ModuleRoot and so only recognized by SimpleDictionary and Dictionary type handlers.
        //
        // "Root" here means via root access, i.e. without an object.
        //
        // Each of these will throw InternalFatalError because they should not be called on type handlers other than
        // SimpleDictionary and Dictionary, both of which provide overrides.
        //
        virtual PropertyIndex GetRootPropertyIndex(PropertyRecord const* propertyRecord) { Throw::FatalInternalError(); }

        virtual BOOL HasRootProperty(DynamicObject* instance, PropertyId propertyId, __out_opt bool *pNoRedecl = nullptr, __out_opt bool *pDeclaredProperty = nullptr) { Throw::FatalInternalError(); }
        virtual BOOL GetRootProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) { Throw::FatalInternalError(); }
        virtual BOOL SetRootProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) { Throw::FatalInternalError(); }
        virtual DescriptorFlags GetRootSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext) { Throw::FatalInternalError(); }
        virtual BOOL DeleteRootProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags flags) { Throw::FatalInternalError(); }

#if DBG
        virtual bool IsLetConstGlobal(DynamicObject* instance, PropertyId propertyId) { Throw::FatalInternalError(); }
#endif
        // Would be nicer to simply pass in lambda callbacks to a Map function here, but virtual methods
        // cannot be templatized and we do not have std::function<> so cannot specify a parameter that will
        // accept lambdas.
        virtual bool NextLetConstGlobal(int& index, RootObjectBase* instance, const PropertyRecord** propertyRecord, Var* value, bool* isConst) { Throw::FatalInternalError(); }
        // ===================================================================================================================

        virtual BOOL IsFixedProperty(const DynamicObject* instance, PropertyId propertyId) { return false; };
        virtual BOOL IsEnumerable(DynamicObject* instance, PropertyId propertyId) = 0;
        virtual BOOL IsWritable(DynamicObject* instance, PropertyId propertyId) = 0;
        virtual BOOL IsConfigurable(DynamicObject* instance, PropertyId propertyId) = 0;
        virtual BOOL SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value) = 0;
        virtual BOOL SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value) = 0;
        virtual BOOL SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value) = 0;

        virtual BOOL HasItem(DynamicObject* instance, uint32 index);
        virtual BOOL SetItem(DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags);
        virtual BOOL SetItemWithAttributes(DynamicObject* instance, uint32 index, Var value, PropertyAttributes attributes);
        virtual BOOL SetItemAttributes(DynamicObject* instance, uint32 index, PropertyAttributes attributes);
        virtual BOOL SetItemAccessors(DynamicObject* instance, uint32 index, Var getter, Var setter);
        virtual BOOL DeleteItem(DynamicObject* instance, uint32 index, PropertyOperationFlags flags);
        virtual BOOL GetItem(DynamicObject* instance, Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext);
        virtual DescriptorFlags GetItemSetter(DynamicObject* instance, uint32 index, Var* setterValue, ScriptContext* requestContext);

        virtual BOOL SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags = PropertyOperation_None) = 0;
        virtual BOOL GetAccessors(DynamicObject* instance, PropertyId propertyId, Var* getter, Var* setter) { return false; };

        virtual BOOL PreventExtensions(DynamicObject *instance) = 0;
        virtual BOOL Seal(DynamicObject *instance) = 0;
        virtual BOOL IsSealed(DynamicObject *instance) { return false; }
        virtual BOOL IsFrozen(DynamicObject *instance) { return false; }
        virtual BOOL SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags = PropertyOperation_None, SideEffects possibleSideEffects = SideEffects_Any) = 0;
        virtual BOOL SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes) = 0;
        virtual BOOL GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes) = 0;

        virtual void ShrinkSlotAndInlineSlotCapacity() { VerifyInlineSlotCapacityIsLocked(); };
        virtual void LockInlineSlotCapacity() { VerifyInlineSlotCapacityIsLocked(); }
        virtual void EnsureInlineSlotCapacityIsLocked() { VerifyInlineSlotCapacityIsLocked(); }
        virtual void VerifyInlineSlotCapacityIsLocked() { Assert(GetIsInlineSlotCapacityLocked()); }

        // ES5Array type handler specific methods. Only implemented by ES5ArrayTypeHandlers.
        virtual bool IsLengthWritable() const { Assert(false); return false; }
        virtual void SetLength(ES5Array* arr, uint32 newLen, PropertyOperationFlags propertyOperationFlags) { Assert(false); }
        virtual BOOL IsObjectArrayFrozen(ES5Array* arr) { Assert(false); return FALSE; }
        virtual BOOL IsItemEnumerable(ES5Array* arr, uint32 index) { Assert(false); return FALSE; }
        virtual BOOL IsValidDescriptorToken(void * descriptorValidationToken) const { Assert(false); return FALSE; }
        virtual uint32 GetNextDescriptor(uint32 key, IndexPropertyDescriptor** descriptor, void ** descriptorValidationToken) { Assert(false); return 0; }
        virtual BOOL GetDescriptor(uint32 index, IndexPropertyDescriptor **ppDescriptor) { Assert(false); return FALSE; }

        // Convert instance type/typeHandler to support SetItem with attribute/getter/setter
        virtual DynamicTypeHandler* ConvertToTypeWithItemAttributes(DynamicObject* instance) = 0;

    private:
        template<bool isStoreField>
        void InvalidateInlineCachesForAllProperties(ScriptContext* requestContext);

    public:
        void InvalidateProtoCachesForAllProperties(ScriptContext* requestContext);
        void InvalidateStoreFieldCachesForAllProperties(ScriptContext* requestContext);

        // For changing __proto__
        void RemoveFromPrototype(DynamicObject* instance, ScriptContext * requestContext);
        void AddToPrototype(DynamicObject* instance, ScriptContext * requestContext);
        virtual void SetPrototype(DynamicObject* instance, RecyclableObject* newPrototype);

        virtual void ResetTypeHandler(DynamicObject * instance);
        virtual void SetAllPropertiesToUndefined(DynamicObject* instance, bool invalidateFixedFields) = 0;
        virtual void MarshalAllPropertiesToScriptContext(DynamicObject* instance, ScriptContext* targetScriptContext, bool invalidateFixedFields) = 0;

        virtual BOOL IsDeferredTypeHandler() const { return FALSE; }
        virtual BOOL IsPathTypeHandler() const { return FALSE; }
        virtual BOOL IsSimpleDictionaryTypeHandler() const {return FALSE; }
        virtual BOOL IsDictionaryTypeHandler() const {return FALSE;}

        static bool IsolatePrototypes() { return CONFIG_FLAG(IsolatePrototypes); }
        static bool ChangeTypeOnProto() { return CONFIG_FLAG(ChangeTypeOnProto); }
        static bool ShouldFixMethodProperties() { return !PHASE_OFF1(FixMethodPropsPhase); }
        static bool ShouldFixDataProperties() { return !PHASE_OFF1(FixDataPropsPhase); }
        static bool ShouldFixAccessorProperties() { return !PHASE_OFF1(FixAccessorPropsPhase); }
        static bool ShouldFixAnyProperties() { return ShouldFixDataProperties() || ShouldFixMethodProperties() || ShouldFixAccessorProperties(); }
        static bool AreSingletonInstancesNeeded() { return ShouldFixAnyProperties(); }

        virtual void SetIsPrototype(DynamicObject* instance) = 0;

#if DBG
        virtual bool SupportsPrototypeInstances() const { return false; }
        virtual bool RespectsIsolatePrototypes() const { return true; }
        virtual bool RespectsChangeTypeOnProto() const { return true; }
#endif

        virtual bool HasSingletonInstance() const { return false; }
        virtual bool TryUseFixedProperty(PropertyRecord const* propertyRecord, Var* pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext);
        virtual bool TryUseFixedAccessor(PropertyRecord const* propertyRecord, Var* pAccessor, FixedPropertyKind propertyType, bool getter, ScriptContext * requestContext);

#if DBG
        virtual bool CanStorePropertyValueDirectly(const DynamicObject* instance, PropertyId propertyId, bool allowLetConst) { return false; }
        virtual bool CheckFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, ScriptContext * requestContext) { return false; };
        virtual bool HasAnyFixedProperties() const { return false; }
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        virtual void DumpFixedFields() const {};
#endif

    public:
        virtual RecyclerWeakReference<DynamicObject>* GetSingletonInstance() const { return nullptr; }

        bool SetSingletonInstanceIfNeeded(DynamicObject* instance)
        {
            if (AreSingletonInstancesNeeded() && CanBeSingletonInstance(instance))
            {
                SetSingletonInstance(instance->CreateWeakReferenceToSelf());
                return true;
            }
            return false;
        }

        void SetSingletonInstanceIfNeeded(RecyclerWeakReference<DynamicObject>* instance)
        {
            if (AreSingletonInstancesNeeded())
            {
                SetSingletonInstance(instance);
            }
        }

        void SetSingletonInstance(RecyclerWeakReference<DynamicObject>* instance)
        {
            Assert(AreSingletonInstancesNeeded());
            SetSingletonInstanceUnchecked(instance);
        }

        virtual void SetSingletonInstanceUnchecked(RecyclerWeakReference<DynamicObject>* instance) { Assert(false); }
        virtual void ClearSingletonInstance() { Assert(false); }

    public:
        static void AdjustSlots_Jit(DynamicObject *const object, const PropertyIndex newInlineSlotCapacity, const int newAuxSlotCapacity);
        static void AdjustSlots(DynamicObject *const object, const PropertyIndex newInlineSlotCapacity, const int newAuxSlotCapacity);

        BigPropertyIndex GetPropertyIndexFromInlineSlotIndex(uint inlininlineSlotIndexeSlot);
        BigPropertyIndex GetPropertyIndexFromAuxSlotIndex(uint auxIndex);

    protected:
        void SetPropertyUpdateSideEffect(DynamicObject* instance, PropertyId propertyId, Var value, SideEffects possibleSideEffects);
        bool VerifyIsExtensible(ScriptContext* scriptContext, bool alwaysThrow);

        void SetOffsetOfInlineSlots(const uint16 offsetOfInlineSlots) { this->offsetOfInlineSlots = offsetOfInlineSlots; }

        void SetInlineSlotCapacity(int16 newInlineSlotCapacity) { this->inlineSlotCapacity = newInlineSlotCapacity; }
        void SetSlotCapacity(int newSlotCapacity) { this->slotCapacity = newSlotCapacity; }

    private:
        virtual BOOL FreezeImpl(DynamicObject *instance, bool isConvertedType) = 0;
    };
}

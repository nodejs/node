//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
class ScriptSite;
namespace Js
{

#if !defined(USED_IN_STATIC_LIB)
#define DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(T) \
    friend class Js::CrossSiteObject<T>; \
    virtual void MarshalToScriptContext(Js::ScriptContext * scriptContext) \
    { \
        Assert(this->GetScriptContext() != scriptContext); \
        AssertMsg(VirtualTableInfo<T>::HasVirtualTable(this), "Derived class need to define marshal to script context"); \
        VirtualTableInfo<Js::CrossSiteObject<T>>::SetVirtualTable(this); \
    }
#else
#define DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(T)  \
        virtual void MarshalToScriptContext(Js::ScriptContext * scriptContext)  {Assert(FALSE);}
#endif

#if DBG
#define SetSlotArguments(propertyId, slotIndex, value) propertyId, false, slotIndex, value
#define SetSlotArgumentsRoot(propertyId, allowLetConst, slotIndex, value) propertyId, allowLetConst, slotIndex, value
#else
#define SetSlotArguments(propertyId, slotIndex, value) slotIndex, value
#define SetSlotArgumentsRoot(propertyId, allowLetConst, slotIndex, value) slotIndex, value
#endif

    enum class DynamicObjectFlags : uint16
    {
        None = 0u,
        ObjectArrayFlagsTag = 1u << 0,       // Tag bit used to indicate the objectArrayOrFlags field is used as flags as opposed to object array pointer.
        HasSegmentMap = 1u << 1,
        HasNoMissingValues = 1u << 2,        // The head segment of a JavascriptArray has no missing values.

        InitialArrayValue = ObjectArrayFlagsTag | HasNoMissingValues,

        AllArrayFlags = HasNoMissingValues | HasSegmentMap,
        AllFlags = ObjectArrayFlagsTag | HasNoMissingValues | HasSegmentMap
    };
    ENUM_CLASS_HELPERS(DynamicObjectFlags, uint16);

    class DynamicObject : public RecyclableObject
    {
        friend class CrossSite;
        friend class DynamicTypeHandler;
        template <typename T> friend class DynamicObjectEnumeratorBase;
        template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics> friend class DynamicObjectEnumerator;
        friend class RecyclableObject;
        friend struct InlineCache;
        friend class ForInObjectEnumerator; // for cache enumerator

        friend class JavascriptOperators; // for ReplaceType
        friend class PathTypeHandlerBase; // for ReplaceType
        friend class JavascriptLibrary;  // for ReplaceType
        friend class ScriptFunction; // for ReplaceType;
        friend class JSON::JSONParser; //for ReplaceType

    private:
        Var* auxSlots;
        // The objectArrayOrFlags field can store one of two things:
        //   a) a pointer to the object array holding numeric properties of this object, or
        //   b) a bitfield of flags.
        // Because object arrays are not commonly used, the storage space can be reused to carry information that
        // can improve performance for typical objects. To indicate the bitfield usage we set the least significant bit to 1.
        // Object array pointer always trumps the flags, such that when the first numeric property is added to an
        // object, its flags will be wiped out.  Hence flags can only be used as a form of cache to improve performance.
        // For functional correctness, some other fallback mechanism must exist to convey the information contained in flags.
        // This fields always starts off initialized to null.  Currently, only JavascriptArray overrides it to store flags, the
        // bits it uses are DynamicObjectFlags::AllArrayFlags.

        union
        {
            ArrayObject * objectArray;          // Only if !IsAnyArray
            struct                                  // Only if IsAnyArray
            {
                DynamicObjectFlags arrayFlags;
                ProfileId arrayCallSiteIndex;
            };
        };

        CompileAssert(sizeof(ProfileId) == 2);
        CompileAssert(static_cast<intptr_t>(DynamicObjectFlags::ObjectArrayFlagsTag) != 0);

        void InitSlots(DynamicObject * instance, ScriptContext * scriptContext);
        void SetTypeHandler(DynamicTypeHandler * typeHandler, bool hasChanged);
        void ReplaceType(DynamicType * type);

    protected:
        DEFINE_VTABLE_CTOR(DynamicObject, RecyclableObject);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(DynamicObject);

        DynamicObject(DynamicType * type, const bool initSlots = true);
        DynamicObject(DynamicType * type, ScriptContext * scriptContext);

        // For boxing stack instance
        DynamicObject(DynamicObject * instance);

        DynamicTypeHandler * GetTypeHandler() const;
        uint16 GetOffsetOfInlineSlots() const;

        template <class T>
        static T* NewObject(Recycler * recycler, DynamicType * type);

    public:
        static DynamicObject * New(Recycler * recycler, DynamicType * type);

        static bool Is(Var aValue);
        static DynamicObject* FromVar(Var value);

        void EnsureSlots(int oldCount, int newCount, ScriptContext * scriptContext, DynamicTypeHandler * newTypeHandler = nullptr);
        void EnsureSlots(int newCount, ScriptContext *scriptContext);

        Var GetSlot(int index);
        Var GetInlineSlot(int index);
        Var GetAuxSlot(int index);

#if DBG
        void SetSlot(PropertyId propertyId, bool allowLetConst, int index, Var value);
        void SetInlineSlot(PropertyId propertyId, bool allowLetConst, int index, Var value);
        void SetAuxSlot(PropertyId propertyId, bool allowLetConst, int index, Var value);
#else
        void SetSlot(int index, Var value);
        void SetInlineSlot(int index, Var value);
        void SetAuxSlot(int index, Var value);
#endif

    private:
        bool IsObjectHeaderInlinedTypeHandlerUnchecked() const;
    public:
        bool IsObjectHeaderInlinedTypeHandler() const;
        bool DeoptimizeObjectHeaderInlining();

    public:
        bool HasNonEmptyObjectArray() const;
        DynamicType * GetDynamicType() const { return (DynamicType *)this->GetType(); }

        // Check if a typeId is of any array type (JavascriptArray or ES5Array).
        static bool IsAnyArrayTypeId(TypeId typeId);

        // Check if a Var is either a JavascriptArray* or ES5Array*.
        static bool IsAnyArray(const Var aValue);

        bool UsesObjectArrayOrFlagsAsFlags() const
        {
            return !!(arrayFlags & DynamicObjectFlags::ObjectArrayFlagsTag);
        }

        ArrayObject* GetObjectArray() const
        {
            return HasObjectArray() ? GetObjectArrayOrFlagsAsArray() : nullptr;
        }

        bool HasObjectArray() const
        {
            // Only JavascriptArray uses the objectArrayOrFlags as flags.
            Assert(DynamicObject::IsAnyArray((Var)this) || !UsesObjectArrayOrFlagsAsFlags() || IsObjectHeaderInlinedTypeHandler());
            return ((objectArray != nullptr) && !UsesObjectArrayOrFlagsAsFlags() && !IsObjectHeaderInlinedTypeHandler());
        }

        ArrayObject* GetObjectArrayUnchecked() const
        {
            return HasObjectArrayUnchecked() ? GetObjectArrayOrFlagsAsArray() : nullptr;
        }

        bool HasObjectArrayUnchecked() const
        {
            return ((objectArray != nullptr) && !UsesObjectArrayOrFlagsAsFlags() && !IsObjectHeaderInlinedTypeHandlerUnchecked());
        }

        BOOL HasObjectArrayItem(uint32 index);
        BOOL DeleteObjectArrayItem(uint32 index, PropertyOperationFlags flags);
        BOOL GetObjectArrayItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext);
        DescriptorFlags GetObjectArrayItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext);
        BOOL SetObjectArrayItem(uint32 index, Var value, PropertyOperationFlags flags);
        BOOL SetObjectArrayItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes);
        BOOL SetObjectArrayItemAttributes(uint32 index, PropertyAttributes attributes);
        BOOL SetObjectArrayItemWritable(PropertyId propertyId, BOOL writable);
        BOOL SetObjectArrayItemAccessors(uint32 index, Var getter, Var setter);
        void InvalidateHasOnlyWritableDataPropertiesInPrototypeChainCacheIfPrototype();
        void ResetObject(DynamicType* type, BOOL keepProperties);

        virtual void SetIsPrototype();

        bool HasLockedType() const;
        bool HasSharedType() const;
        bool HasSharedTypeHandler() const;
        bool LockType();
        bool ShareType();
        bool GetIsExtensible() const;
        bool GetHasNoEnumerableProperties();
        bool SetHasNoEnumerableProperties(bool value);
        virtual bool HasReadOnlyPropertiesInvisibleToTypeHandler() { return false; }

        void InitSlots(DynamicObject* instance);
        virtual int GetPropertyCount() override;
        virtual PropertyId GetPropertyId(PropertyIndex index) override;
        virtual PropertyId GetPropertyId(BigPropertyIndex index) override;
        PropertyIndex GetPropertyIndex(PropertyId propertyId) sealed;
        virtual BOOL HasProperty(PropertyId propertyId) override;
        virtual BOOL HasOwnProperty(PropertyId propertyId) override;
        virtual BOOL GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetInternalProperty(Var instance, PropertyId internalPropertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL SetInternalProperty(PropertyId internalPropertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual DescriptorFlags GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual DescriptorFlags GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags = PropertyOperation_None, PropertyValueInfo* info = nullptr) override;
        virtual BOOL SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags = PropertyOperation_None, SideEffects possibleSideEffects = SideEffects_Any) override;
        virtual BOOL DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags) override;
        virtual BOOL IsFixedProperty(PropertyId propertyId) override;
        virtual BOOL HasItem(uint32 index) override;
        virtual BOOL HasOwnItem(uint32 index) override;
        virtual BOOL GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override;
        virtual BOOL GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override;
        virtual DescriptorFlags GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext) override;
        virtual BOOL SetItem(uint32 index, Var value, PropertyOperationFlags flags) override;
        virtual BOOL DeleteItem(uint32 index, PropertyOperationFlags flags) override;
        virtual BOOL ToPrimitive(JavascriptHint hint, Var* result, ScriptContext * requestContext) override;
        virtual BOOL GetEnumerator(BOOL enumNonEnumerable, Var* enumerator, ScriptContext* scriptContext, bool preferSnapshotSemantics = true, bool enumSymbols = false) override;
        virtual BOOL SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags = PropertyOperation_None) override;
        virtual BOOL GetAccessors(PropertyId propertyId, Var *getter, Var *setter, ScriptContext * requestContext) override;
        virtual BOOL IsWritable(PropertyId propertyId) override;
        virtual BOOL IsConfigurable(PropertyId propertyId) override;
        virtual BOOL IsEnumerable(PropertyId propertyId) override;
        virtual BOOL SetEnumerable(PropertyId propertyId, BOOL value) override;
        virtual BOOL SetWritable(PropertyId propertyId, BOOL value) override;
        virtual BOOL SetConfigurable(PropertyId propertyId, BOOL value) override;
        virtual BOOL SetAttributes(PropertyId propertyId, PropertyAttributes attributes) override;
        virtual BOOL IsExtensible() override { return GetIsExtensible(); };
        virtual BOOL PreventExtensions() override;
        virtual BOOL Seal() override;
        virtual BOOL Freeze() override;
        virtual BOOL IsSealed() override;
        virtual BOOL IsFrozen() override;
        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual Var GetTypeOfString(ScriptContext * requestContext) override;

#if DBG
        virtual bool CanStorePropertyValueDirectly(PropertyId propertyId, bool allowLetConst) override;
#endif

        virtual void RemoveFromPrototype(ScriptContext * requestContext) override;
        virtual void AddToPrototype(ScriptContext * requestContext) override;
        virtual void SetPrototype(RecyclableObject* newPrototype) override;

        virtual BOOL IsCrossSiteObject() const { return FALSE; }

        virtual DynamicType* DuplicateType();
        static bool IsTypeHandlerCompatibleForObjectHeaderInlining(DynamicTypeHandler * oldTypeHandler, DynamicTypeHandler * newTypeHandler);

        void ChangeType();

        void ChangeTypeIf(const Type* oldType);

        Var GetNextProperty(PropertyIndex& index, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols = false);
        Var GetNextProperty(BigPropertyIndex& index, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols = false);

        BOOL FindNextProperty(PropertyIndex& index, JavascriptString** propertyString, PropertyId* propertyId, PropertyAttributes* attributes, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols = false) const;
        BOOL FindNextProperty(BigPropertyIndex& index, JavascriptString** propertyString, PropertyId* propertyId, PropertyAttributes* attributes, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols = false) const;

        virtual BOOL HasDeferredTypeHandler() const sealed;
        static DWORD GetOffsetOfAuxSlots();
        static DWORD GetOffsetOfObjectArray();
        static DWORD GetOffsetOfType();

        Js::BigPropertyIndex GetPropertyIndexFromInlineSlotIndex(uint inlineSlotIndex);
        Js::BigPropertyIndex GetPropertyIndexFromAuxSlotIndex(uint auxIndex);
        BOOL GetAttributesWithPropertyIndex(PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes);

        RecyclerWeakReference<DynamicObject>* CreateWeakReferenceToSelf();

        void SetObjectArray(ArrayObject* objectArray);
    protected:
        // These are only call for arrays
        void InitArrayFlags(DynamicObjectFlags flags);
        DynamicObjectFlags GetArrayFlags() const;
        DynamicObjectFlags GetArrayFlags_Unchecked() const; // do not use except in extreme circumstances
        void SetArrayFlags(const DynamicObjectFlags flags);

        ProfileId GetArrayCallSiteIndex() const;
        void SetArrayCallSiteIndex(ProfileId profileId);

        static DynamicObject * BoxStackInstance(DynamicObject * instance);
    private:
        ArrayObject* EnsureObjectArray();
        ArrayObject* GetObjectArrayOrFlagsAsArray() const { return objectArray; }

        template <PropertyId propertyId>
        BOOL ToPrimitiveImpl(Var* result, ScriptContext * requestContext);
        BOOL CallToPrimitiveFunction(Var toPrimitiveFunction, PropertyId propertyId, Var* result, ScriptContext * requestContext);
#if DBG
    public:
        virtual bool DbgIsDynamicObject() const override { return true; }
#endif

#ifdef RECYCLER_STRESS
    public:
        virtual void Finalize(bool isShutdown) override;
        virtual void Dispose(bool isShutdown) override;
        virtual void Mark(Recycler *recycler) override;
#endif
    };
} // namespace Js

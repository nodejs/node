//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(DynamicType);
    DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(DynamicType);

    DynamicType::DynamicType(DynamicType * type, DynamicTypeHandler *typeHandler, bool isLocked, bool isShared)
        : Type(type), typeHandler(typeHandler), isLocked(isLocked), isShared(isShared)
    {
        Assert(!this->isLocked || this->typeHandler->GetIsLocked());
        Assert(!this->isShared || this->typeHandler->GetIsShared());
    }


    DynamicType::DynamicType(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint, DynamicTypeHandler * typeHandler, bool isLocked, bool isShared)
        : Type(scriptContext, typeId, prototype, entryPoint) , typeHandler(typeHandler), isLocked(isLocked), isShared(isShared), hasNoEnumerableProperties(false)
    {
        Assert(typeHandler != nullptr);
        Assert(!this->isLocked || this->typeHandler->GetIsLocked());
        Assert(!this->isShared || this->typeHandler->GetIsShared());
    }

    DynamicType *
    DynamicType::New(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint, DynamicTypeHandler * typeHandler, bool isLocked, bool isShared)
    {
        return RecyclerNew(scriptContext->GetRecycler(), DynamicType, scriptContext, typeId, prototype, entryPoint, typeHandler, isLocked, isShared);
    }

    bool
    DynamicType::Is(TypeId typeId)
    {
        return !StaticType::Is(typeId);
    }

    bool
    DynamicType::SetHasNoEnumerableProperties(bool value)
    {
        if (!value)
        {
            this->hasNoEnumerableProperties = value;
            return false;
        }

#if DEBUG
        PropertyIndex propertyIndex = (PropertyIndex)-1;
        JavascriptString* propertyString = nullptr;
        PropertyId propertyId = Constants::NoProperty;
        Assert(!this->GetTypeHandler()->FindNextProperty(this->GetScriptContext(), propertyIndex, &propertyString, &propertyId, nullptr, this, this, true));
#endif

        this->hasNoEnumerableProperties = true;
        return true;
    }

    void DynamicType::PrepareForTypeSnapshotEnumeration()
    {
        if(!GetIsLocked() && CONFIG_FLAG(TypeSnapshotEnumeration))
        {
            // Lock the type and handler, enabling us to enumerate properties of the type snapshotted
            // at the beginning of enumeration, despite property changes made by script during enumeration.
            LockType(); // Note: this only works for type handlers that support locking.
        }
    }

    void DynamicObject::InitSlots(DynamicObject* instance)
    {
        InitSlots(instance, GetScriptContext());
    }

    void DynamicObject::InitSlots(DynamicObject * instance, ScriptContext * scriptContext)
    {
        Recycler * recycler = scriptContext->GetRecycler();
        int slotCapacity = GetTypeHandler()->GetSlotCapacity();
        int inlineSlotCapacity = GetTypeHandler()->GetInlineSlotCapacity();
        if (slotCapacity > inlineSlotCapacity)
        {
            instance->auxSlots = RecyclerNewArrayZ(recycler, Var, slotCapacity - inlineSlotCapacity);
        }
    }

    int DynamicObject::GetPropertyCount()
    {
        if (!this->GetTypeHandler()->EnsureObjectReady(this))
        {
            return 0;
        }
        return GetTypeHandler()->GetPropertyCount();
    }

    PropertyId DynamicObject::GetPropertyId(PropertyIndex index)
    {
        return GetTypeHandler()->GetPropertyId(this->GetScriptContext(), index);
    }

    PropertyId DynamicObject::GetPropertyId(BigPropertyIndex index)
    {
        return GetTypeHandler()->GetPropertyId(this->GetScriptContext(), index);
    }

    PropertyIndex DynamicObject::GetPropertyIndex(PropertyId propertyId)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        Assert(propertyId != Constants::NoProperty);
        return GetTypeHandler()->GetPropertyIndex(this->GetScriptContext()->GetPropertyName(propertyId));
    }

    BOOL DynamicObject::HasProperty(PropertyId propertyId)
    {
        // HasProperty can be invoked with propertyId = NoProperty in some cases, namely cross-thread and DOM
        // This is done to force creation of a type handler in case the type handler is deferred
        Assert(!Js::IsInternalPropertyId(propertyId) || propertyId == Js::Constants::NoProperty);
        return GetTypeHandler()->HasProperty(this, propertyId);
    }

    BOOL DynamicObject::HasOwnProperty(PropertyId propertyId)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return HasProperty(propertyId);
    }

    BOOL DynamicObject::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetProperty(this, originalInstance, propertyId, value, info, requestContext);
    }

    BOOL DynamicObject::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetProperty");

        return GetTypeHandler()->GetProperty(this, originalInstance, propertyNameString, value, info, requestContext);
    }

    BOOL DynamicObject::GetInternalProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        Assert(Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetProperty(this, originalInstance, propertyId, value, nullptr, nullptr);
    }

    BOOL DynamicObject::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetProperty(this, originalInstance, propertyId, value, info, requestContext);
    }

    BOOL DynamicObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->SetProperty(this, propertyId, value, flags, info);
    }

    BOOL DynamicObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling SetProperty");

        return GetTypeHandler()->SetProperty(this, propertyNameString, value, flags, info);
    }

    BOOL DynamicObject::SetInternalProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        Assert(Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->SetProperty(this, propertyId, value, flags, nullptr);
    }

    DescriptorFlags DynamicObject::GetSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetSetter(this, propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags DynamicObject::GetSetter(JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        return GetTypeHandler()->GetSetter(this, propertyNameString, setterValue, info, requestContext);
    }

    BOOL DynamicObject::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->InitProperty(this, propertyId, value, flags, info);
    }

    BOOL DynamicObject::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->DeleteProperty(this, propertyId, flags);
    }

    BOOL DynamicObject::IsFixedProperty(PropertyId propertyId)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->IsFixedProperty(this, propertyId);
    }

    BOOL DynamicObject::HasItem(uint32 index)
    {
        return GetTypeHandler()->HasItem(this, index);
    }

    BOOL DynamicObject::HasOwnItem(uint32 index)
    {
        return HasItem(index);
    }

    BOOL DynamicObject::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {
        return GetTypeHandler()->GetItem(this, originalInstance, index, value, requestContext);
    }

    BOOL DynamicObject::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {
        return GetTypeHandler()->GetItem(this, originalInstance, index, value, requestContext);
    }

    DescriptorFlags DynamicObject::GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {
        return GetTypeHandler()->GetItemSetter(this, index, setterValue, requestContext);
    }

    BOOL DynamicObject::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {
        return GetTypeHandler()->SetItem(this, index, value, flags);
    }

    BOOL DynamicObject::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {
        return GetTypeHandler()->DeleteItem(this, index, flags);
    }

    BOOL DynamicObject::ToPrimitive(JavascriptHint hint, Var* result, ScriptContext * requestContext)
    {
        if(hint == JavascriptHint::HintString)
        {
            return ToPrimitiveImpl<PropertyIds::toString>(result, requestContext)
                || ToPrimitiveImpl<PropertyIds::valueOf>(result, requestContext);
        }
        else
        {
            Assert(hint == JavascriptHint::None || hint == JavascriptHint::HintNumber);
            return ToPrimitiveImpl<PropertyIds::valueOf>(result, requestContext)
                || ToPrimitiveImpl<PropertyIds::toString>(result, requestContext);

        }
    }

    template <PropertyId propertyId>
    BOOL DynamicObject::ToPrimitiveImpl(Var* result, ScriptContext * requestContext)
    {
        CompileAssert(propertyId == PropertyIds::valueOf || propertyId == PropertyIds::toString);
        InlineCache * inlineCache = propertyId == PropertyIds::valueOf ? requestContext->GetValueOfInlineCache() : requestContext->GetToStringInlineCache();
        // Use per script context inline cache for valueOf and toString
        Var aValue = JavascriptOperators::PatchGetValueUsingSpecifiedInlineCache(inlineCache, this, this, propertyId, requestContext);

        // Fast path to the default valueOf/toString implementation
        if (propertyId == PropertyIds::valueOf)
        {
            if (aValue == requestContext->GetLibrary()->GetObjectValueOfFunction())
            {
                Assert(JavascriptConversion::IsCallable(aValue));
                // The default Object.prototype.valueOf will in turn just call ToObject().
                // The result is always an object if it is not undefined or null (which "this" is not)
                return false;
            }
        }
        else
        {
            if (aValue == requestContext->GetLibrary()->GetObjectToStringFunction())
            {
                Assert(JavascriptConversion::IsCallable(aValue));
                // These typeIds should never be here (they override ToPrimitive or they don't derive to DynamicObject::ToPrimitive)
                // Otherwise, they may case implicit call in ToStringHelper
                Assert(this->GetTypeId() != TypeIds_HostDispatch
                    && this->GetTypeId() != TypeIds_HostObject);
                *result = JavascriptObject::ToStringHelper(this, requestContext);
                return true;
            }
        }

        return CallToPrimitiveFunction(aValue, propertyId, result, requestContext);
    }
    BOOL DynamicObject::CallToPrimitiveFunction(Var toPrimitiveFunction, PropertyId propertyId, Var* result, ScriptContext * requestContext)
    {
        if (JavascriptConversion::IsCallable(toPrimitiveFunction))
        {
            RecyclableObject* toStringFunction = RecyclableObject::FromVar(toPrimitiveFunction);

            ThreadContext * threadContext = requestContext->GetThreadContext();
            Var aResult = threadContext->ExecuteImplicitCall(toStringFunction, ImplicitCall_ToPrimitive, [=]() -> Js::Var
            {
                // Stack object should have a pre-op bail on implicit call.  We shouldn't see them here.
                Assert(!ThreadContext::IsOnStack(this) || threadContext->HasNoSideEffect(toStringFunction));
                return toStringFunction->GetEntryPoint()(toStringFunction, CallInfo(CallFlags_Value, 1), this);
            });
 
            if (!aResult)
            {
                // There was an implicit call and implicit calls are disabled. This would typically cause a bailout.
                Assert(threadContext->IsDisableImplicitCall());
                *result = requestContext->GetLibrary()->GetNull();
                return true;
            }

            if (JavascriptOperators::GetTypeId(aResult) <= TypeIds_LastToPrimitiveType)
            {
                *result = aResult;
                return true;
            }
        }
        return false;
    }

    BOOL DynamicObject::GetEnumerator(BOOL enumNonEnumerable, Var* enumerator, ScriptContext * requestContext, bool preferSnapshotSemantics, bool enumSymbols)
    {
        if (!this->GetTypeHandler()->EnsureObjectReady(this))
        {
            *enumerator = nullptr;
            return FALSE;
        }

        // Create the appropriate enumerator object.
        if (preferSnapshotSemantics)
        {
            if (this->GetTypeHandler()->GetPropertyCount() == 0 && !this->HasObjectArray())
            {
                *enumerator = requestContext->GetLibrary()->GetNullEnumerator();
            }
            else
            {
                GetDynamicType()->PrepareForTypeSnapshotEnumeration();
                if (this->GetDynamicType()->GetIsLocked())
                {
                    if (enumSymbols)
                    {
                        if (enumNonEnumerable)
                        {
                            *enumerator = DynamicObjectSnapshotEnumeratorWPCache<BigPropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/true>::New(requestContext, this);
                        }
                        else
                        {
                            *enumerator = DynamicObjectSnapshotEnumeratorWPCache<BigPropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/true>::New(requestContext, this);
                        }
                    }
                    else if (enumNonEnumerable)
                    {
                        *enumerator = DynamicObjectSnapshotEnumeratorWPCache<BigPropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/false>::New(requestContext, this);
                    }
                    else
                    {
                        *enumerator = DynamicObjectSnapshotEnumeratorWPCache<BigPropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/false>::New(requestContext, this);
                    }
                }
                else
                {
                    if (enumSymbols)
                    {
                        if (enumNonEnumerable)
                        {
                            *enumerator = DynamicObjectSnapshotEnumerator<BigPropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/true>::New(requestContext, this);
                        }
                        else
                        {
                            *enumerator = DynamicObjectSnapshotEnumerator<BigPropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/true>::New(requestContext, this);
                        }
                    }
                    else if (enumNonEnumerable)
                    {
                        *enumerator = DynamicObjectSnapshotEnumerator<BigPropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/false>::New(requestContext, this);
                    }
                    else
                    {
                        *enumerator = DynamicObjectSnapshotEnumerator<BigPropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/false>::New(requestContext, this);
                    }
                }
            }
        }
        else if (enumSymbols)
        {
            if (enumNonEnumerable)
            {
                *enumerator = DynamicObjectEnumerator<BigPropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/true, /*snapShotSementics*/false>::New(requestContext, this);
            }
            else
            {
                *enumerator = DynamicObjectEnumerator<BigPropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/true, /*snapShotSementics*/false>::New(requestContext, this);
            }
        }
        else if (enumNonEnumerable)
        {
            *enumerator = DynamicObjectEnumerator<BigPropertyIndex, /*enumNonEnumerable*/true, /*enumSymbols*/false, /*snapShotSementics*/false>::New(requestContext, this);
        }
        else
        {
            *enumerator = DynamicObjectEnumerator<BigPropertyIndex, /*enumNonEnumerable*/false, /*enumSymbols*/false, /*snapShotSementics*/false>::New(requestContext, this);
        }

        return true;
    }

    BOOL DynamicObject::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)

    {
        return GetTypeHandler()->SetAccessors(this, propertyId, getter, setter, flags);
    }

    BOOL DynamicObject::GetAccessors(PropertyId propertyId, Var *getter, Var *setter, ScriptContext * requestContext)
    {
        return GetTypeHandler()->GetAccessors(this, propertyId, getter, setter);
    }

    BOOL DynamicObject::PreventExtensions()
    {
        return GetTypeHandler()->PreventExtensions(this);
    }

    BOOL DynamicObject::Seal()
    {
        return GetTypeHandler()->Seal(this);
    }

    BOOL DynamicObject::Freeze()
    {
        Type* oldType = this->GetType();
        BOOL ret = GetTypeHandler()->Freeze(this);

        // We just made all properties on this object non-writable.
        // Make sure the type is evolved so that the property string caches
        // are no longer hit.
        if (this->GetType() == oldType)
        {
            this->ChangeType();
        }

        return ret;
    }

    BOOL DynamicObject::IsSealed()
    {
        return GetTypeHandler()->IsSealed(this);
    }

    BOOL DynamicObject::IsFrozen()
    {
        return GetTypeHandler()->IsFrozen(this);
    }

    BOOL DynamicObject::IsWritable(PropertyId propertyId)
    {
        return GetTypeHandler()->IsWritable(this, propertyId);
    }

    BOOL DynamicObject::IsConfigurable(PropertyId propertyId)
    {
        return GetTypeHandler()->IsConfigurable(this, propertyId);
    }

    BOOL DynamicObject::IsEnumerable(PropertyId propertyId)
    {
        return GetTypeHandler()->IsEnumerable(this, propertyId);
    }

    BOOL DynamicObject::SetEnumerable(PropertyId propertyId, BOOL value)
    {
        return GetTypeHandler()->SetEnumerable(this, propertyId, value);
    }

    BOOL DynamicObject::SetWritable(PropertyId propertyId, BOOL value)
    {
        return GetTypeHandler()->SetWritable(this, propertyId, value);
    }

    BOOL DynamicObject::SetConfigurable(PropertyId propertyId, BOOL value)
    {
        return GetTypeHandler()->SetConfigurable(this, propertyId, value);
    }

    BOOL DynamicObject::SetAttributes(PropertyId propertyId, PropertyAttributes attributes)
    {
        return GetTypeHandler()->SetAttributes(this, propertyId, attributes);
    }

    BOOL DynamicObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"{...}");
        return TRUE;
    }

    BOOL DynamicObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"Object");
        return TRUE;
    }

    Var DynamicObject::GetTypeOfString(ScriptContext * requestContext)
    {
        return requestContext->GetLibrary()->GetObjectTypeDisplayString();
    }

    // If this object is not extensible and the property being set does not already exist,
    // if throwIfNotExtensible is
    // * true, a type error will be thrown
    // * false, FALSE will be returned (unless strict mode is enabled, in which case a type error will be thrown).
    // Either way, the property will not be set.
    //
    // throwIfNotExtensible should always be false for non-numeric properties.
    BOOL DynamicObject::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        return GetTypeHandler()->SetPropertyWithAttributes(this, propertyId, value, attributes, info, flags, possibleSideEffects);
    }

#if DBG
    bool DynamicObject::CanStorePropertyValueDirectly(PropertyId propertyId, bool allowLetConst)
    {
        return GetTypeHandler()->CanStorePropertyValueDirectly(this, propertyId, allowLetConst);
    }
#endif

    void DynamicObject::RemoveFromPrototype(ScriptContext * requestContext)
    {
        GetTypeHandler()->RemoveFromPrototype(this, requestContext);
    }

    void DynamicObject::AddToPrototype(ScriptContext * requestContext)
    {
        GetTypeHandler()->AddToPrototype(this, requestContext);
    }

    void DynamicObject::SetPrototype(RecyclableObject* newPrototype)
    {
        // Mark newPrototype it is being set as prototype
        newPrototype->SetIsPrototype();

        GetTypeHandler()->SetPrototype(this, newPrototype);
    }
}

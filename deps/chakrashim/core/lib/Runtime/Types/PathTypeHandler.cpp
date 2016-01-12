//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    PathTypeHandlerBase::PathTypeHandlerBase(TypePath* typePath, uint16 pathLength, const PropertyIndex slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked, bool isShared, DynamicType* predecessorType) :
        DynamicTypeHandler(slotCapacity, inlineSlotCapacity, offsetOfInlineSlots, DefaultFlags | (isLocked ? IsLockedFlag : 0) | (isShared ? (MayBecomeSharedFlag | IsSharedFlag) : 0)),
        typePath(typePath),
        predecessorType(predecessorType)
    {
        Assert(pathLength <= slotCapacity);
        Assert(inlineSlotCapacity <= slotCapacity);
        SetUnusedBytesValue(pathLength);
        isNotPathTypeHandlerOrHasUserDefinedCtor = predecessorType == nullptr ? false : predecessorType->GetTypeHandler()->GetIsNotPathTypeHandlerOrHasUserDefinedCtor();
    }

    int PathTypeHandlerBase::GetPropertyCount()
    {
        return GetPathLength();
    }

    PropertyId PathTypeHandlerBase::GetPropertyId(ScriptContext* scriptContext, PropertyIndex index)
    {
        if (index < GetPathLength())
        {
            return typePath->GetPropertyId(index)->GetPropertyId();
        }
        else
        {
            return Constants::NoProperty;
        }
    }

    PropertyId PathTypeHandlerBase::GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index)
    {
        if (index < GetPathLength())
        {
            return typePath->GetPropertyId(index)->GetPropertyId();
        }
        else
        {
            return Constants::NoProperty;
        }
    }

    BOOL PathTypeHandlerBase::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyStringName, PropertyId* propertyId,
        PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        Assert(propertyStringName);
        Assert(propertyId);
        Assert(type);

        for (; index < GetPathLength(); ++index)
        {
            const PropertyRecord* propertyRecord = typePath->GetPropertyId(index);

            // Skip this property if it is a symbol and we are not including symbol properties
            if (!enumSymbols && propertyRecord->IsSymbol())
            {
                continue;
            }

            if (attributes)
            {
                *attributes = PropertyEnumerable;
            }

            *propertyId = propertyRecord->GetPropertyId();
            PropertyString* propertyString = type->GetScriptContext()->GetPropertyString(*propertyId);
            *propertyStringName = propertyString;

            uint16 inlineOrAuxSlotIndex;
            bool isInlineSlot;
            PropertyIndexToInlineOrAuxSlotIndex(index, &inlineOrAuxSlotIndex, &isInlineSlot);

            propertyString->UpdateCache(type, inlineOrAuxSlotIndex, isInlineSlot,
                !FixPropsOnPathTypes() || (this->GetPathLength() < this->typePath->GetMaxInitializedLength() && !this->typePath->GetIsFixedFieldAt(index, this->GetPathLength())));

            return TRUE;
        }

        return FALSE;
    }

    PropertyIndex PathTypeHandlerBase::GetPropertyIndex(const PropertyRecord* propertyRecord)
    {
        return typePath->LookupInline(propertyRecord->GetPropertyId(), GetPathLength());
    }

    PropertyIndex PathTypeHandlerBase::GetPropertyIndex(PropertyId propertyId)
    {
        return typePath->LookupInline(propertyId, GetPathLength());
    }

    bool PathTypeHandlerBase::GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info)
    {
        Js::PropertyIndex absSlotIndex = typePath->LookupInline(propertyRecord->GetPropertyId(), GetPathLength());
        info.slotIndex = AdjustSlotIndexForInlineSlots(absSlotIndex);
        info.isAuxSlot = absSlotIndex >= this->inlineSlotCapacity;
        info.isWritable = info.slotIndex != Constants::NoSlot;
        return info.slotIndex != Constants::NoSlot;
    }

    bool PathTypeHandlerBase::IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex)
    {
        uint propertyCount = record.propertyCount;
        Js::EquivalentPropertyEntry* properties = record.properties;
        for (uint pi = 0; pi < propertyCount; pi++)
        {
            const EquivalentPropertyEntry* entry = &properties[pi];
            if (!this->PathTypeHandlerBase::IsObjTypeSpecEquivalent(type, entry))
            {
                failedPropertyIndex = pi;
                return false;
            }
        }

        return true;
    }

    bool PathTypeHandlerBase::IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry *entry)
    {
        Js::PropertyIndex absSlotIndex = typePath->LookupInline(entry->propertyId, GetPathLength());

        if (absSlotIndex != Constants::NoSlot)
        {
            Js::PropertyIndex relSlotIndex = AdjustValidSlotIndexForInlineSlots(absSlotIndex);
            if (relSlotIndex != entry->slotIndex || ((absSlotIndex >= GetInlineSlotCapacity()) != entry->isAuxSlot))
            {
                return false;
            }

            int maxInitializedLength = this->typePath->GetMaxInitializedLength();
            if (entry->mustBeWritable && FixPropsOnPathTypes() && (absSlotIndex >= maxInitializedLength || this->typePath->GetIsFixedFieldAt(absSlotIndex, this->GetPathLength())))
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

    BOOL PathTypeHandlerBase::HasProperty(DynamicObject* instance, PropertyId propertyId, __out_opt bool *noRedecl)
    {
        uint32 indexVal;
        if (noRedecl != nullptr)
        {
            *noRedecl = false;
        }

        if (PathTypeHandlerBase::GetPropertyIndex(propertyId) != Constants::NoSlot)
        {
            return true;
        }

        // Check numeric propertyId only if objectArray is available
        ScriptContext* scriptContext = instance->GetScriptContext();
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return PathTypeHandlerBase::HasItem(instance, indexVal);
        }

        return false;
    }

    BOOL PathTypeHandlerBase::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {
        // Consider: Implement actual string hash lookup
        PropertyRecord const* propertyRecord;
        instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return PathTypeHandlerBase::HasProperty(instance, propertyRecord->GetPropertyId());
    }

    BOOL PathTypeHandlerBase::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        PropertyIndex index = typePath->LookupInline(propertyId, GetPathLength());
        if (index != Constants::NoSlot)
        {
            *value = instance->GetSlot(index);
            PropertyValueInfo::Set(info, instance, index);
            if (FixPropsOnPathTypes() && (index >= this->typePath->GetMaxInitializedLength() || this->typePath->GetIsFixedFieldAt(index, GetPathLength())))
            {
                PropertyValueInfo::DisableStoreFieldCache(info);
            }
            return true;
        }

        // Check numeric propertyId only if objectArray available
        uint32 indexVal;
        ScriptContext* scriptContext = instance->GetScriptContext();
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return PathTypeHandlerBase::GetItem(instance, originalInstance, indexVal, value, requestContext);
        }

        return false;
    }

    BOOL PathTypeHandlerBase::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        // Consider: Implement actual string hash lookup
        Assert(requestContext);
        PropertyRecord const* propertyRecord;
        wchar_t const * propertyName = propertyNameString->GetString();
        charcount_t const propertyNameLength = propertyNameString->GetLength();

        if (instance->HasObjectArray())
        {
            requestContext->GetOrAddPropertyRecord(propertyName, propertyNameLength, &propertyRecord);
        }
        else
        {
            requestContext->FindPropertyRecord(propertyName, propertyNameLength, &propertyRecord);
            if (propertyRecord == nullptr)
            {
                return false;
            }
        }
        return PathTypeHandlerBase::GetProperty(instance, originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext);
    }

    BOOL PathTypeHandlerBase::SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        return SetPropertyInternal(instance, propertyId, value, info, flags, SideEffects_Any);
    }

    BOOL PathTypeHandlerBase::SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        // Consider: Implement actual string hash lookup
        PropertyRecord const* propertyRecord;
        instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return PathTypeHandlerBase::SetProperty(instance, propertyRecord->GetPropertyId(), value, flags, info);
    }

    BOOL PathTypeHandlerBase::SetPropertyInternal(DynamicObject* instance, PropertyId propertyId, Var value, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        // Path type handler doesn't support pre-initialization (PropertyOperation_PreInit). Pre-initialized properties
        // will get marked as fixed when pre-initialized and then as non-fixed when their actual values are set.

        Assert(value != nullptr || IsInternalPropertyId(propertyId));
        PropertyIndex index = PathTypeHandlerBase::GetPropertyIndex(propertyId);

        if (index != Constants::NoSlot)
        {
            // If type is shared then the handler must be shared as well.  This is a weaker invariant than in AddPropertyInternal,
            // because the type coming in here may be the result of DynamicObject::ChangeType(). In that case the handler may have
            // already been shared, but the newly created type isn't - and likely never will be - shared (is typically unreachable).
            // In CacheOperators::CachePropertyWrite we ensure that we never cache property adds for types that aren't shared.
            Assert(!instance->GetDynamicType()->GetIsShared() || GetIsShared());

            Assert(instance->GetDynamicType()->GetIsShared() == GetIsShared());

            bool populateInlineCache = GetIsShared() ||
                ProcessFixedFieldChange(instance, propertyId, index, value, (flags & PropertyOperation_NonFixedValue) != 0);

            SetSlotUnchecked(instance, index, value);

            if (populateInlineCache)
            {
                Assert((instance->GetDynamicType()->GetIsShared()) || (FixPropsOnPathTypes() && instance->GetDynamicType()->GetTypeHandler()->GetIsOrMayBecomeShared()));
                // Can't assert the following.  With NewScObject we can jump to the type handler at the tip (where the singleton is),
                // even though we haven't yet initialized the properties all the way to the tip, and we don't want to kill
                // the singleton in that case yet.  It's basically a transient inconsistent state, but we have to live with it.
                // The user's code will never see the object in this state.
                //Assert(!instance->GetTypeHandler()->HasSingletonInstance());
                PropertyValueInfo::Set(info, instance, index);
            }
            else
            {
                PropertyValueInfo::SetNoCache(info, instance);
            }

            SetPropertyUpdateSideEffect(instance, propertyId, value, possibleSideEffects);
            return true;
        }

        // Always check numeric propertyId. This may create an objectArray.
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 indexVal;
        if (scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return PathTypeHandlerBase::SetItem(instance, indexVal, value, PropertyOperation_None);
        }

        return PathTypeHandlerBase::AddPropertyInternal(instance, propertyId, value, info, flags, possibleSideEffects);
    }

    BOOL PathTypeHandlerBase::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags flags)
    {
#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertPathToDictionaryCount2++;
#endif
        // Check numeric propertyId only if objectArray available
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 indexVal;
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return PathTypeHandlerBase::DeleteItem(instance, indexVal, flags);
        }

        return  ConvertToSimpleDictionaryType(instance, GetPathLength())->DeleteProperty(instance, propertyId, flags);
    }

    BOOL PathTypeHandlerBase::IsFixedProperty(const DynamicObject* instance, PropertyId propertyId)
    {
        if (!FixPropsOnPathTypes())
        {
            return false;
        }

        PropertyIndex index = PathTypeHandlerBase::GetPropertyIndex(propertyId);
        Assert(index != Constants::NoSlot);

        return this->typePath->GetIsFixedFieldAt(index, GetPathLength());
    }

    BOOL PathTypeHandlerBase::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {
        return true;
    }

    BOOL PathTypeHandlerBase::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {
        return true;
    }

    BOOL PathTypeHandlerBase::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {
        return true;
    }

    BOOL PathTypeHandlerBase::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertPathToDictionaryCount3++;
#endif
        return value || ConvertToSimpleDictionaryType(instance, GetPathLength())->SetEnumerable(instance, propertyId, value);
    }

    BOOL PathTypeHandlerBase::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertPathToDictionaryCount3++;
#endif
        return value || ConvertToSimpleDictionaryType(instance, GetPathLength())->SetWritable(instance, propertyId, value);
    }

    BOOL PathTypeHandlerBase::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertPathToDictionaryCount3++;
#endif
        return value || ConvertToSimpleDictionaryType(instance, GetPathLength())->SetConfigurable(instance, propertyId, value);
    }

    BOOL PathTypeHandlerBase::SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {
#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertPathToDictionaryCount4++;
#endif
        return ConvertToDictionaryType(instance)->SetAccessors(instance, propertyId, getter, setter, flags);
    }

    BOOL PathTypeHandlerBase::PreventExtensions(DynamicObject* instance)
    {
#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertPathToDictionaryCount4++;
#endif
        BOOL tempResult = this->ConvertToSharedNonExtensibleTypeIfNeededAndCallOperation(instance, InternalPropertyRecords::NonExtensibleType,
            [&](SimpleDictionaryTypeHandlerWithNontExtensibleSupport* newTypeHandler)
            {
                return newTypeHandler->PreventExtensionsInternal(instance);
            });

        Assert(tempResult);
        if (tempResult)
        {
            // Call preventExtensions on the objectArray -- which will create different type for array type handler.
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray)
            {
                objectArray->PreventExtensions();
            }
        }

        return tempResult;
    }

    BOOL PathTypeHandlerBase::Seal(DynamicObject* instance)
    {
#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertPathToDictionaryCount4++;
#endif
        // For seal we need an array with non-default attributes, which is ES5Array,
        // and in current design ES5Array goes side-by-side with DictionaryTypeHandler.
        // Note that 2 instances can have same PathTypehandler but still different objectArray items, e.g. {x:0, 0:0} and {x:0, 1:0}.
        // Technically we could change SimpleDictionaryTypehandler to override *Item* methods,
        // similar to DictionaryTypeHandler, but objects with numeric properties are currently seen as low priority,
        // so just don't share the type.
        if (instance->HasObjectArray())
        {
            return this->ConvertToDictionaryType(instance)->Seal(instance);
        }
        else
        {
            return this->ConvertToSharedNonExtensibleTypeIfNeededAndCallOperation(instance, InternalPropertyRecords::SealedType,
                [&](SimpleDictionaryTypeHandlerWithNontExtensibleSupport* newTypeHandler)
                {
                    return newTypeHandler->SealInternal(instance);
                });
        }
    }

    BOOL PathTypeHandlerBase::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {
#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertPathToDictionaryCount4++;
#endif
        // See the comment inside Seal WRT HasObjectArray branch.
        if (instance->HasObjectArray())
        {
            return this->ConvertToDictionaryType(instance)->Freeze(instance, isConvertedType);
        }
        else
        {
            return this->ConvertToSharedNonExtensibleTypeIfNeededAndCallOperation(instance, InternalPropertyRecords::FrozenType,
                [&](SimpleDictionaryTypeHandlerWithNontExtensibleSupport* newTypeHandler)
                {
                    return newTypeHandler->FreezeInternal(instance, true);  // true: we don't want to change type in FreezeInternal.
                });
        }
    }

    // Checks whether conversion to shared type is needed and performs it, then calls actual operation on the shared type.
    // Template method used for PreventExtensions, Seal, Freeze.
    // Parameters:
    // - instance: object instance to operate on.
    // - operationInternalPropertyRecord: the internal property record for preventExtensions/seal/freeze.
    // - FType: functor/lambda to perform actual forced operation (such as PreventExtensionsInternal) on the shared type.
    template<typename FType>
    BOOL PathTypeHandlerBase::ConvertToSharedNonExtensibleTypeIfNeededAndCallOperation(DynamicObject* instance, const PropertyRecord* operationInternalPropertyRecord, FType operation)
    {
        AssertMsg(operationInternalPropertyRecord == InternalPropertyRecords::NonExtensibleType ||
            operationInternalPropertyRecord == InternalPropertyRecords::SealedType ||
            operationInternalPropertyRecord == InternalPropertyRecords::FrozenType,
            "Wrong/unsupported value of operationInternalPropertyRecord.");

        RecyclerWeakReference<DynamicType>* newTypeWeakRef = nullptr;
        DynamicType * oldType = instance->GetDynamicType();

        // See if we already have shared type for this type and convert to it, otherwise create a new one.
        if (!GetSuccessor(operationInternalPropertyRecord, &newTypeWeakRef) || newTypeWeakRef->Get() == nullptr)
        {
            // Convert to new shared type with shared simple dictionary type handler and call operation on it.
            SimpleDictionaryTypeHandlerWithNontExtensibleSupport* newTypeHandler = ConvertToSimpleDictionaryType
                <SimpleDictionaryTypeHandlerWithNontExtensibleSupport>(instance, this->GetPathLength(), true);

            Assert(newTypeHandler->GetMayBecomeShared() && !newTypeHandler->GetIsShared());
            DynamicType* newType = instance->GetDynamicType();
            newType->LockType();
            Assert(!newType->GetIsShared());

            ScriptContext * scriptContext = instance->GetScriptContext();
            Recycler * recycler = scriptContext->GetRecycler();
            SetSuccessor(oldType, operationInternalPropertyRecord, recycler->CreateWeakReferenceHandle<DynamicType>(newType), scriptContext);
            return operation(newTypeHandler);
        }
        else
        {
            DynamicType* newType = newTypeWeakRef->Get();
            DynamicTypeHandler* newTypeHandler = newType->GetTypeHandler();

            // Consider: Consider doing something special for frozen objects, whose values cannot
            // change and so we could retain them as fixed, even when the type becomes shared.
            newType->ShareType();
            // Consider: If we isolate prototypes, we should never get here with the prototype flag set.
            // There should be nothing to transfer.
            // Assert(!IsolatePrototypes() || (this->GetFlags() & IsPrototypeFlag) == 0);
            newTypeHandler->SetFlags(IsPrototypeFlag, this->GetFlags());
            Assert(!newTypeHandler->HasSingletonInstance());

            if(instance->IsObjectHeaderInlinedTypeHandler())
            {
                const PropertyIndex newInlineSlotCapacity = newTypeHandler->GetInlineSlotCapacity();
                AdjustSlots(instance, newInlineSlotCapacity, newTypeHandler->GetSlotCapacity() - newInlineSlotCapacity);
            }
            ReplaceInstanceType(instance, newType);
        }

        return TRUE;
    }

    DynamicType* PathTypeHandlerBase::PromoteType(DynamicObject* instance, const PropertyRecord* propertyRecord, PropertyIndex* propertyIndex)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        DynamicType* currentType = instance->GetDynamicType();

        DynamicType* nextType = this->PromoteType<false>(currentType, propertyRecord, false, scriptContext, instance, propertyIndex);
        PathTypeHandlerBase* nextPath = (PathTypeHandlerBase*) nextType->GetTypeHandler();

        instance->EnsureSlots(this->GetSlotCapacity(), nextPath->GetSlotCapacity(), scriptContext, nextType->GetTypeHandler());

        ReplaceInstanceType(instance, nextType);
        return nextType;
    }

    template <typename T>
    T* PathTypeHandlerBase::ConvertToTypeHandler(DynamicObject* instance)
    {
        Assert(instance);
        ScriptContext* scriptContext = instance->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();

        PathTypeHandlerBase * oldTypeHandler;

        // Ideally 'this' and oldTypeHandler->GetTypeHandler() should be same
        // But we can have calls from external DOM objects, which requests us to replace the type of the
        // object with a new type. And in such cases, this API gets called with oldTypeHandler and the
        // new type (obtained from the External DOM object)
        // We use the duplicated typeHandler, if we deOptimized the object successfully, else we retain the earlier
        // behavior of using 'this' pointer.

        if (instance->DeoptimizeObjectHeaderInlining())
        {
            oldTypeHandler = reinterpret_cast<PathTypeHandlerBase *>(instance->GetTypeHandler());
        }
        else
        {
            oldTypeHandler = this;
        }

        Assert(oldTypeHandler);

        T* newTypeHandler = RecyclerNew(recycler, T, recycler, oldTypeHandler->GetSlotCapacity(), oldTypeHandler->GetInlineSlotCapacity(), oldTypeHandler->GetOffsetOfInlineSlots());
        // We expect the new type handler to start off marked as having only writable data properties.
        Assert(newTypeHandler->GetHasOnlyWritableDataProperties());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        DynamicType* oldType = instance->GetDynamicType();
        RecyclerWeakReference<DynamicObject>* oldSingletonInstance = oldTypeHandler->GetSingletonInstance();
        oldTypeHandler->TraceFixedFieldsBeforeTypeHandlerChange(L"converting", L"PathTypeHandler", L"DictionaryTypeHandler", instance, oldTypeHandler, oldType, oldSingletonInstance);
#endif

        bool const canBeSingletonInstance = DynamicTypeHandler::CanBeSingletonInstance(instance);
        // If this type had been installed on a stack instance it shouldn't have a singleton Instance
        Assert(canBeSingletonInstance || !oldTypeHandler->HasSingletonInstance());

        // This instance may not be the singleton instance for this handler. There may be a singleton at the tip
        // and this instance may be getting initialized via an object literal and one of the properties may
        // be an accessor.  In this case we will convert to a DictionaryTypeHandler and it's correct to
        // transfer this instance, even tough different from the singleton. Ironically, this instance
        // may even appear to be at the tip along with the other singleton, because the set of properties (by
        // name, not value) may be identical.
        // Consider: Consider threading PropertyOperation_Init through InitProperty and SetAccessors,
        // to be sure that we don't assert only in this narrow case.
        // Assert(this->typePath->GetSingletonInstance() == instance);

        Assert(oldTypeHandler->HasSingletonInstanceOnlyIfNeeded());

        // Don't install stack instance as singleton instance
        if (canBeSingletonInstance)
        {
            if (DynamicTypeHandler::AreSingletonInstancesNeeded())
            {
                RecyclerWeakReference<DynamicObject>* curSingletonInstance = oldTypeHandler->typePath->GetSingletonInstance();
                if (curSingletonInstance != nullptr && curSingletonInstance->Get() == instance)
                {
                    newTypeHandler->SetSingletonInstance(curSingletonInstance);
                }
                else
                {
                    newTypeHandler->SetSingletonInstance(instance->CreateWeakReferenceToSelf());
                }
            }
        }

        bool transferFixed = canBeSingletonInstance;

        // If we are a prototype or may become a prototype we must transfer used as fixed bits.  See point 4 in ConvertToSimpleDictionaryType.
        Assert(!DynamicTypeHandler::IsolatePrototypes() || ((oldTypeHandler->GetFlags() & IsPrototypeFlag) == 0));
        bool transferUsedAsFixed = ((oldTypeHandler->GetFlags() & IsPrototypeFlag) != 0 || (oldTypeHandler->GetIsOrMayBecomeShared() && !DynamicTypeHandler::IsolatePrototypes()));

        for (PropertyIndex i = 0; i < oldTypeHandler->GetPathLength(); i++)
        {
            // Consider: As noted in point 2 in ConvertToSimpleDictionaryType, when converting to non-shared handler we could be more
            // aggressive and mark every field as fixed, because we will always take a type transition. We have to remember to respect
            // the switches as to which kinds of properties we should fix, and for that we need the values from the instance. Even if
            // the type handler says the property is initialized, the current instance may not have a value for it. Check for value != null.
            if (PathTypeHandlerBase::FixPropsOnPathTypes())
            {
                TypePath * typePath = oldTypeHandler->typePath;
                newTypeHandler->Add(typePath->GetPropertyId(i), PropertyDynamicTypeDefaults,
                    i < typePath->GetMaxInitializedLength(),
                    transferFixed && typePath->GetIsFixedFieldAt(i, oldTypeHandler->GetPathLength()),
                    transferUsedAsFixed && typePath->GetIsUsedFixedFieldAt(i, oldTypeHandler->GetPathLength()),
                    scriptContext);
            }
            else
            {
                newTypeHandler->Add(oldTypeHandler->typePath->GetPropertyId(i), PropertyDynamicTypeDefaults, true, false, false, scriptContext);
            }
        }

#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
        if (PathTypeHandlerBase::FixPropsOnPathTypes())
        {
            Assert(oldTypeHandler->HasSingletonInstanceOnlyIfNeeded());
            oldTypeHandler->typePath->ClearSingletonInstanceIfSame(instance);
        }
#endif

        // PathTypeHandlers are always shared, so if we're isolating prototypes, a PathTypeHandler should
        // never have the prototype flag set.
        Assert(!DynamicTypeHandler::IsolatePrototypes() || ((oldTypeHandler->GetFlags() & IsPrototypeFlag) == 0));
        AssertMsg(!newTypeHandler->GetIsPrototype(), "Why did we create a brand new type handler with a prototype flag set?");
        newTypeHandler->SetFlags(IsPrototypeFlag, oldTypeHandler->GetFlags());

        // Any new type handler we expect to see here should have inline slot capacity locked.  If this were to change, we would need
        // to update our shrinking logic (see ShrinkSlotAndInlineSlotCapacity).
        Assert(newTypeHandler->GetIsInlineSlotCapacityLocked());
        newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection, oldTypeHandler->GetPropertyTypes());
        newTypeHandler->SetInstanceTypeHandler(instance);
        Assert(!newTypeHandler->HasSingletonInstance() || !instance->HasSharedType());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        PathTypeHandlerBase::TraceFixedFieldsAfterTypeHandlerChange(instance, oldTypeHandler, newTypeHandler, oldType, instance->GetDynamicType(), oldSingletonInstance);
#endif

        return newTypeHandler;
    }

    DictionaryTypeHandler* PathTypeHandlerBase::ConvertToDictionaryType(DynamicObject* instance)
    {
        return ConvertToTypeHandler<DictionaryTypeHandler>(instance);
    }

    ES5ArrayTypeHandler* PathTypeHandlerBase::ConvertToES5ArrayType(DynamicObject* instance)
    {
        return ConvertToTypeHandler<ES5ArrayTypeHandler>(instance);
    }

    template <typename T>
    T* PathTypeHandlerBase::ConvertToSimpleDictionaryType(DynamicObject* instance, int propertyCapacity, bool mayBecomeShared)
    {
        Assert(instance);
        ScriptContext* scriptContext = instance->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();

        // Ideally 'this' and oldTypeHandler->GetTypeHandler() should be same
        // But we can have calls from external DOM objects, which requests us to replace the type of the
        // object with a new type. And in such cases, this API gets called with oldTypeHandler and the
        // new type (obtained from the External DOM object)
        // We use the duplicated typeHandler, if we deOptimized the object successfully, else we retain the earlier
        // behavior of using 'this' pointer.

        PathTypeHandlerBase * oldTypeHandler = nullptr;

        if (instance->DeoptimizeObjectHeaderInlining())
        {
            Assert(instance->GetTypeHandler()->IsPathTypeHandler());
            oldTypeHandler = reinterpret_cast<PathTypeHandlerBase *>(instance->GetTypeHandler());
        }
        else
        {
            oldTypeHandler = this;
        }

        Assert(oldTypeHandler);

        DynamicType* oldType = instance->GetDynamicType();
        T* newTypeHandler = RecyclerNew(recycler, T, recycler, oldTypeHandler->GetSlotCapacity(), propertyCapacity, oldTypeHandler->GetInlineSlotCapacity(), oldTypeHandler->GetOffsetOfInlineSlots());
        // We expect the new type handler to start off marked as having only writable data properties.
        Assert(newTypeHandler->GetHasOnlyWritableDataProperties());

        // Care must be taken to correctly set up fixed field bits whenever a type's handler is changed.  Exactly what needs to
        // be done depends on whether the current handler is shared, whether the new handler is shared, whether the current
        // handler has the prototype flag set, and even whether we take a type transition as part of the process.
        //
        // 1. Can we set fixed bits on new handler for the fields that are marked as fixed on current handler?
        //
        //    Yes, if the new type handler is not shared.  If the handler is not shared, we know that only this instance will
        //    ever use it.  Otherwise, a different instance could transition to the same type handler, but have different values
        //    for fields marked as fixed.
        //
        // 2. Can we set fixed bits on new handler even for the fields that are not marked as fixed on current handler?
        //
        //    Yes, if the new type handler is not shared and we take a type transition during conversion.  The first condition
        //    is required for the same reason as in point 1 above.  The type transition is needed to ensure that any store
        //    field fast paths for this instance get invalidated.  If they didn't, then the newly fixed field could get
        //    overwritten on the fast path without triggering necessary invalidation.
        //
        //    Note that it's desirable to mark additional fields as fixed (particularly when the instance becomes a prototype)
        //    to counteract the effect of false type sharing, which may unnecessarily turn off some fixed field bits.
        //
        // 3. Do we need to clear any fixed field bits on the old or new type handler?
        //
        //    Yes, we must clear fixed fields bits for properties that aren't also used as fixed, but only if both type handlers
        //    are shared and we don't isolate prototypes.  This is rather tricky and results from us pre-creating certain handlers
        //    even before any instances actually have values for all represented properties.  We must avoid the situation, in which
        //    one instance switched to a new type handler with some fixed field not yet used as fixed, and later the second
        //    instance follows the same handler evolution with the same field used as fixed.  Upon switching to the new handler
        //    the second instance would "forget" that the field was used as fixed and fail to invalidate when overwritten.
        //
        //    Example: Instance A with TH1 has a fixed method FOO, which has not been used as fixed yet.  Then instance B gets
        //    pre-created and lands on TH1 (and so far assumes FOO is fixed).  As B's pre-creation continues, it moves to TH2, but
        //    thus far FOO has not been used as fixed.  Now instance A becomes a prototype, and its method FOO is used in a hard-coded
        //    JIT sequence, thus marking it as used as fixed.  Instance A then transitions to TH2 and we lose track of FOO being used
        //    as fixed.  If FOO is then overwritten on A, the hard-coded JIT sequence does not get invalidated and continues to call
        //    the old method FOO.
        //
        // 4. Can we avoid setting used as fixed bits on new handler for fields marked as used as fixed on current handler?
        //
        //    Yes, if the current type handler doesn't have the prototype flag and current handler is not shared or new handler
        //    is not shared or we isolate prototypes, and we take a type transition as part of the conversion.
        //
        //    Type transition ensures that any field loads from the instance are invalidated (including
        //    any that may have hard-coded the fixed field's value).  Hence, if the fixed field on this instance were to be later
        //    overwritten it will not cause any functional issues.  On the other hand, field loads from prototype are not affected
        //    by the prototype object's type change.  Therefore, if this instance is a prototype we must carry the used as fixed
        //    bits forward to ensure that if we overwrite any fixed field we explicitly trigger invalidation.
        //
        //    Review: Actually, the comment below is overly conservative.  If the second instance that became a prototype
        //    followed the same type evolution path, it would have to have invalidated all fixed fields, so there should be no need
        //    to transfer used as fixed bits, unless the current instance is already a prototype.
        //    In addition, if current handler is shared and the new handler is shared, a different instance with the current handler
        //    may later become a prototype (if we don't isolate prototypes) and follow the same conversion to the new handler, even
        //    if the current instance is not a prototype.  Hence, the new type handler must retain the used as fixed bits, so that
        //    proper invalidation can be triggered later, if overwritten.
        //
        //    Note that this may lead to the new type handler with some fields not marked as fixed, but marked as used as fixed.
        //
        //    Note also that if we isolate prototypes, we guarantee that no prototype instance will share a type handler with any
        //    other instance.  Hence, the problem sequence above could not take place.
        //
        // 5. Do we need to invalidate JIT-ed code for any fields marked as used as fixed on current handler?
        //
        //    No.  With the rules above any necessary invalidation will be triggered when the value actually gets overwritten.
        //

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        RecyclerWeakReference<DynamicObject>* oldSingletonInstance = oldTypeHandler->GetSingletonInstance();
        oldTypeHandler->TraceFixedFieldsBeforeTypeHandlerChange(L"converting", L"PathTypeHandler", L"SimpleDictionaryTypeHandler", instance, oldTypeHandler, oldType, oldSingletonInstance);
#endif

        bool const canBeSingletonInstance = DynamicTypeHandler::CanBeSingletonInstance(instance);
        // If this type had been installed on a stack instance it shouldn't have a singleton Instance
        Assert(canBeSingletonInstance || !oldTypeHandler->HasSingletonInstance());

        // Consider: It looks like we're delaying sharing of these type handlers until the second instance arrives, so we could
        // set the singleton here and zap it later.
        if (!mayBecomeShared && canBeSingletonInstance)
        {
            Assert(oldTypeHandler->HasSingletonInstanceOnlyIfNeeded());
            if (DynamicTypeHandler::AreSingletonInstancesNeeded())
            {
                RecyclerWeakReference<DynamicObject>* curSingletonInstance = oldTypeHandler->typePath->GetSingletonInstance();
                if (curSingletonInstance != nullptr && curSingletonInstance->Get() == instance)
                {
                    newTypeHandler->SetSingletonInstance(curSingletonInstance);
                }
                else
                {
                    newTypeHandler->SetSingletonInstance(instance->CreateWeakReferenceToSelf());
                }
            }
        }

        // It would be nice to transfer fixed fields if the new type handler may become fixed later (but isn't yet).  This would allow
        // singleton instances to retain fixed fields.  It would require that when we do actually share the target type (when the second
        // instance arrives), we clear (and invalidate, if necessary) any fixed fields.  This may be a reasonable trade-off.
        bool transferIsFixed = !mayBecomeShared && canBeSingletonInstance;

        // If we are a prototype or may become a prototype we must transfer used as fixed bits.  See point 4 above.
        Assert(!DynamicTypeHandler::IsolatePrototypes() || ((oldTypeHandler->GetFlags() & IsPrototypeFlag) == 0));
        // For the global object we don't emit a type check before a hard-coded use of a fixed field.  Therefore a type transition isn't sufficient to
        // invalidate any used fixed fields, and we must continue tracking them on the new type handler.  The global object should never have a path
        // type handler.
        Assert(instance->GetTypeId() != TypeIds_GlobalObject);
        // If the type isn't locked, we may not change the type of the instance, and we must also track the used fixed fields on the new handler.
        bool transferUsedAsFixed = !instance->GetDynamicType()->GetIsLocked() || ((oldTypeHandler->GetFlags() & IsPrototypeFlag) != 0 || (oldTypeHandler->GetIsOrMayBecomeShared() && !DynamicTypeHandler::IsolatePrototypes()));

        // Consider: As noted in point 2 above, when converting to non-shared SimpleDictionaryTypeHandler we could be more aggressive
        // and mark every field as fixed, because we will always take a type transition.  We have to remember to respect the switches as
        // to which kinds of properties we should fix, and for that we need the values from the instance.  Even if the type handler
        // says the property is initialized, the current instance may not have a value for it.  Check for value != null.
        for (PropertyIndex i = 0; i < oldTypeHandler->GetPathLength(); i++)
        {
            if (PathTypeHandlerBase::FixPropsOnPathTypes())
            {
                Js::TypePath * typePath = oldTypeHandler->typePath;
                newTypeHandler->Add(typePath->GetPropertyId(i), PropertyDynamicTypeDefaults,
                    i < typePath->GetMaxInitializedLength(),
                    transferIsFixed && typePath->GetIsFixedFieldAt(i, GetPathLength()),
                    transferUsedAsFixed && typePath->GetIsUsedFixedFieldAt(i, GetPathLength()),
                    scriptContext);
            }
            else
            {
                newTypeHandler->Add(oldTypeHandler->typePath->GetPropertyId(i), PropertyDynamicTypeDefaults, true, false, false, scriptContext);
            }

            // No need to clear fixed fields not used as fixed, because we never convert during pre-creation of type handlers and we always
            // add properties in order they appear on the type path.  Hence, any existing fixed fields will be turned off by any other
            // instance following this type path.  See point 3 above.
        }

#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
        // Clear the singleton from this handler regardless of mayBecomeShared, because this instance no longer uses this handler.
        if (PathTypeHandlerBase::FixPropsOnPathTypes())
        {
            Assert(oldTypeHandler->HasSingletonInstanceOnlyIfNeeded());
            oldTypeHandler->typePath->ClearSingletonInstanceIfSame(instance);
        }
#endif

        if (mayBecomeShared)
        {
            newTypeHandler->SetFlags(IsLockedFlag | MayBecomeSharedFlag);
        }

        Assert(!DynamicTypeHandler::IsolatePrototypes() || !oldTypeHandler->GetIsOrMayBecomeShared() || ((oldTypeHandler->GetFlags() & IsPrototypeFlag) == 0));
        AssertMsg((newTypeHandler->GetFlags() & IsPrototypeFlag) == 0, "Why did we create a brand new type handler with a prototype flag set?");
        newTypeHandler->SetFlags(IsPrototypeFlag, oldTypeHandler->GetFlags());

        // Any new type handler we expect to see here should have inline slot capacity locked.  If this were to change, we would need
        // to update our shrinking logic (see ShrinkSlotAndInlineSlotCapacity).
        Assert(newTypeHandler->GetIsInlineSlotCapacityLocked());
        newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection, oldTypeHandler->GetPropertyTypes());
        newTypeHandler->SetInstanceTypeHandler(instance);
        Assert(!newTypeHandler->HasSingletonInstance() || !instance->HasSharedType());
        // We assumed that we don't need to transfer used as fixed bits unless we are a prototype, which is only valid if we also changed the type.
        Assert(transferUsedAsFixed || (instance->GetType() != oldType && oldType->GetTypeId() != TypeIds_GlobalObject));

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        PathTypeHandlerBase::TraceFixedFieldsAfterTypeHandlerChange(instance, oldTypeHandler, newTypeHandler, oldType, instance->GetDynamicType(), oldSingletonInstance);
#endif

#ifdef PROFILE_TYPES
        scriptContext->convertPathToSimpleDictionaryCount++;
#endif
        return newTypeHandler;
    }

    BOOL PathTypeHandlerBase::SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        if (attributes == PropertyDynamicTypeDefaults)
        {
            return PathTypeHandlerBase::SetPropertyInternal(instance, propertyId, value, info, flags, possibleSideEffects);
        }
        else
        {
            return ConvertToSimpleDictionaryType(instance, GetPathLength() + 1)->SetPropertyWithAttributes(instance, propertyId, value, attributes, info, flags, possibleSideEffects);
        }
    }

    BOOL PathTypeHandlerBase::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {
        if ( (attributes & PropertyDynamicTypeDefaults) != PropertyDynamicTypeDefaults)
        {
#ifdef PROFILE_TYPES
            instance->GetScriptContext()->convertPathToDictionaryCount3++;
#endif

            return ConvertToSimpleDictionaryType(instance, GetPathLength())->SetAttributes(instance, propertyId, attributes);
        }

        return true;
    }

    BOOL PathTypeHandlerBase::GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {
        if (index < this->GetPathLength())
        {
            Assert(this->GetPropertyId(instance->GetScriptContext(), index) == propertyId);
            *attributes = PropertyDynamicTypeDefaults;
            return true;
        }
        return false;
    }

    bool PathTypeHandlerBase::UsePathTypeHandlerForObjectLiteral(
        const PropertyIdArray *const propIds,
        ScriptContext *const scriptContext,
        bool *const check__proto__Ref)
    {
        Assert(propIds);
        Assert(scriptContext);

        // Always check __proto__ entry, now that object literals always honor __proto__
        const bool check__proto__ = propIds->has__proto__ && scriptContext->GetConfig()->Is__proto__Enabled();
        if(check__proto__Ref)
        {
            *check__proto__Ref = check__proto__;
        }

        return !check__proto__ && propIds->count < TypePath::MaxPathTypeHandlerLength && !propIds->hadDuplicates;
    }

    DynamicType* PathTypeHandlerBase::CreateTypeForNewScObject(ScriptContext* scriptContext, DynamicType* type, const Js::PropertyIdArray *propIds, bool shareType)
    {
        uint count = propIds->count;

        bool check__proto__;
        if (UsePathTypeHandlerForObjectLiteral(propIds, scriptContext, &check__proto__))
        {
#ifdef PROFILE_OBJECT_LITERALS
            scriptContext->objectLiteralCount[count]++;
#endif
            for (uint i = 0; i < count; i++)
            {
                PathTypeHandlerBase *pathHandler = (PathTypeHandlerBase *)type->typeHandler;
                Js::PropertyId propertyId = propIds->elements[i];

                PropertyIndex propertyIndex = pathHandler->GetPropertyIndex(propertyId);

                if (propertyIndex != Constants::NoSlot)
                {
                    continue;
                }

#ifdef PROFILE_OBJECT_LITERALS
                {
                    RecyclerWeakReference<DynamicType>* nextTypeWeakRef;
                    if (!pathHandler->GetSuccessor(scriptContext->GetPropertyName(propertyId), &nextTypeWeakRef) || nextTypeWeakRef->Get() == nullptr)
                    {
                        scriptContext->objectLiteralPathCount++;
                    }
                }
#endif
                type = pathHandler->PromoteType<true>(type, scriptContext->GetPropertyName(propertyId), shareType, scriptContext, nullptr, &propertyIndex);
            }
        }
        else if (count <= static_cast<uint>(SimpleDictionaryTypeHandler::MaxPropertyIndexSize))
        {
            type = SimpleDictionaryTypeHandler::CreateTypeForNewScObject(scriptContext, type, propIds, shareType, check__proto__);
        }
        else if (count <= static_cast<uint>(BigSimpleDictionaryTypeHandler::MaxPropertyIndexSize))
        {
            type = BigSimpleDictionaryTypeHandler::CreateTypeForNewScObject(scriptContext, type, propIds, shareType, check__proto__);
        }
        else
        {
            Throw::OutOfMemory();
        }

        return type;
    }

    DynamicType *
    PathTypeHandlerBase::CreateNewScopeObject(ScriptContext *scriptContext, DynamicType *type, const PropertyIdArray *propIds, PropertyAttributes extraAttributes, uint extraAttributesSlotCount)
    {
        uint count = propIds->count;

        Recycler* recycler = scriptContext->GetRecycler();

        SimpleDictionaryTypeHandler* typeHandler = SimpleDictionaryTypeHandler::New(recycler, count, 0, 0, true, true);

        for (uint i = 0; i < count; i++)
        {
            PropertyId propertyId = propIds->elements[i];
            const PropertyRecord* propertyRecord = propertyId == Constants::NoProperty ? NULL : scriptContext->GetPropertyName(propertyId);
            // This will add the property as initialized and non-fixed.  That's fine because we will populate the property values on the
            // scope object right after this (see JavascriptOperators::OP_InitCachedScope).  We will not treat these properties as fixed.
            PropertyAttributes attributes = PropertyWritable | PropertyEnumerable;
            if (i < extraAttributesSlotCount)
            {
                attributes |= extraAttributes;
            }
            typeHandler->Add(propertyRecord, attributes, scriptContext);
        }
        AssertMsg((typeHandler->GetFlags() & IsPrototypeFlag) == 0, "Why does a newly created type handler have a IsPrototypeFlag set?");

 #ifdef PROFILE_OBJECT_LITERALS
        scriptContext->objectLiteralSimpleDictionaryCount++;
 #endif

        type = RecyclerNew(recycler, DynamicType, type, typeHandler, /* isLocked = */ true, /* isShared = */ true);

        return type;
    }

    template <bool isObjectLiteral>
    DynamicType* PathTypeHandlerBase::PromoteType(DynamicType* predecessorType, const PropertyRecord* propertyRecord, bool shareType, ScriptContext* scriptContext, DynamicObject* instance, PropertyIndex* propertyIndex)
    {
        Assert(propertyIndex != nullptr);
        Assert(isObjectLiteral || instance != nullptr);

        Recycler* recycler = scriptContext->GetRecycler();
        PropertyIndex index;
        DynamicType * nextType;
        RecyclerWeakReference<DynamicType>* nextTypeWeakRef = nullptr;

        PathTypeHandlerBase * nextPath;
        if (!GetSuccessor(propertyRecord, &nextTypeWeakRef) || nextTypeWeakRef->Get() == nullptr)
        {

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            DynamicType* oldType = predecessorType;
            RecyclerWeakReference<DynamicObject>* oldSingletonInstance = GetSingletonInstance();
            bool branching = typePath->GetPathLength() > GetPathLength();
            TraceFixedFieldsBeforeTypeHandlerChange(branching ? L"branching" : L"advancing", L"PathTypeHandler", L"PathTypeHandler", instance, this, oldType, oldSingletonInstance);
#endif

            TypePath * newTypePath = typePath;

            if (typePath->GetPathLength() > GetPathLength())
            {
                // We need to branch the type path.

                newTypePath = typePath->Branch(recycler, GetPathLength(), GetIsOrMayBecomeShared() && !IsolatePrototypes());

#ifdef PROFILE_TYPES
                scriptContext->branchCount++;
#endif
#ifdef PROFILE_OBJECT_LITERALS
                if (isObjectLiteral)
                {
                    scriptContext->objectLiteralBranchCount++;
                }
#endif
            }
            else if (typePath->GetPathLength() == typePath->GetPathSize())
            {
                // We need to grow the type path.

                newTypePath = typePath->Grow(recycler);

                // Update all the predecessor types that use this TypePath to the new TypePath.
                // This will allow the old TypePath to be collected, and will ensure that the
                // fixed field info is correct for those types.

                PathTypeHandlerBase * typeHandlerToUpdate = this;
                TypePath * oldTypePath = typePath;
                while (true)
                {
                    typeHandlerToUpdate->typePath = newTypePath;

                    DynamicType * nextType = typeHandlerToUpdate->GetPredecessorType();
                    if (nextType == nullptr)
                    {
                        break;
                    }

                    Assert(nextType->GetTypeHandler()->IsPathTypeHandler());
                    typeHandlerToUpdate = (PathTypeHandlerBase *)nextType->GetTypeHandler();
                    if (typeHandlerToUpdate->typePath != oldTypePath)
                    {
                        break;
                    }
                }
            }

            index = (PropertyIndex)newTypePath->AddInternal(propertyRecord);

            const PropertyIndex newPropertyCount = GetPathLength() + 1;
            const PropertyIndex newSlotCapacity = max(newPropertyCount, static_cast<PropertyIndex>(GetSlotCapacity()));
            PropertyIndex newInlineSlotCapacity = GetInlineSlotCapacity();
            uint16 newOffsetOfInlineSlots = GetOffsetOfInlineSlots();
            if(IsObjectHeaderInlinedTypeHandler() && newSlotCapacity > GetSlotCapacity())
            {
                newInlineSlotCapacity -= GetObjectHeaderInlinableSlotCapacity();
                newOffsetOfInlineSlots = sizeof(DynamicObject);
            }
            bool markTypeAsShared = !FixPropsOnPathTypes() || shareType;
            nextPath = SimplePathTypeHandler::New(scriptContext, newTypePath, newPropertyCount, newSlotCapacity, newInlineSlotCapacity, newOffsetOfInlineSlots, true, markTypeAsShared, predecessorType);
            if (!markTypeAsShared) nextPath->SetMayBecomeShared();
            Assert(nextPath->GetHasOnlyWritableDataProperties());
            nextPath->CopyPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection, GetPropertyTypes());
            nextPath->SetPropertyTypes(PropertyTypesInlineSlotCapacityLocked, GetPropertyTypes());

            if (shareType)
            {
                nextPath->AddBlankFieldAt(propertyRecord->GetPropertyId(), index, scriptContext);
            }

#ifdef PROFILE_TYPES
            scriptContext->maxPathLength = max(GetPathLength() + 1, scriptContext->maxPathLength);
#endif

            if (isObjectLiteral)
            {
                // The new type isn't shared yet.  We will make it shared when the second instance attains it.
                nextType = RecyclerNew(recycler, DynamicType, predecessorType, nextPath, /* isLocked = */ true, /* isShared = */ markTypeAsShared);
            }
            else
            {
                // The new type isn't shared yet.  We will make it shared when the second instance attains it.
                nextType = instance->DuplicateType();
                nextType->typeHandler = nextPath;
                markTypeAsShared ? nextType->SetIsLockedAndShared() : nextType->SetIsLocked();
            }

            SetSuccessor(predecessorType, propertyRecord, recycler->CreateWeakReferenceHandle<DynamicType>(nextType), scriptContext);
            // We just extended the current type path to a new tip or created a brand new type path.  We should
            // be at the tip of the path and there should be no instances there yet.
            Assert(nextPath->GetPathLength() == newTypePath->GetPathLength());
            Assert(!FixPropsOnPathTypes() || shareType || nextPath->GetPathLength() > newTypePath->GetMaxInitializedLength());

#ifdef PROFILE_TYPES
            scriptContext->promoteCount++;
#endif
#ifdef PROFILE_OBJECT_LITERALS
            if (isObjectLiteral)
            {
                scriptContext->objectLiteralPromoteCount++;
            }
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            TraceFixedFieldsAfterTypeHandlerChange(instance, this, nextPath, oldType, nextType, oldSingletonInstance);
#endif
        }
        else
        {
#ifdef PROFILE_TYPES
            scriptContext->cacheCount++;
#endif

            // Now that the second (or subsequent) instance reached this type, make sure that it's shared.
            nextType = nextTypeWeakRef->Get();
            nextPath = (PathTypeHandlerBase *)nextType->GetTypeHandler();
            Assert(nextPath->GetIsInlineSlotCapacityLocked() == this->GetIsInlineSlotCapacityLocked());

            index = nextPath->GetPropertyIndex(propertyRecord);

            Assert((FixPropsOnPathTypes() && nextPath->GetMayBecomeShared()) || (nextPath->GetIsShared() && nextType->GetIsShared()));
            if (FixPropsOnPathTypes() && !nextType->GetIsShared())
            {
                if (!nextPath->GetIsShared())
                {
                    nextPath->AddBlankFieldAt(propertyRecord->GetPropertyId(), index, scriptContext);
                    nextPath->DoShareTypeHandlerInternal<false>(scriptContext);
                }
                nextType->ShareType();
            }
        }

        Assert(!IsolatePrototypes() || !GetIsOrMayBecomeShared() || !GetIsPrototype());
        nextPath->SetFlags(IsPrototypeFlag, this->GetFlags());
        Assert(this->GetHasOnlyWritableDataProperties() == nextPath->GetHasOnlyWritableDataProperties());
        Assert(this->GetIsInlineSlotCapacityLocked() == nextPath->GetIsInlineSlotCapacityLocked());
        nextPath->SetPropertyTypes(PropertyTypesWritableDataOnlyDetection, this->GetPropertyTypes());

        (*propertyIndex) = index;

        return nextType;
    }

    void
    PathTypeHandlerBase::ResetTypeHandler(DynamicObject * instance)
    {
        // The type path is allocated in the type allocator associated with the script context.
        // So we can't reuse it in other context.  Just convert the type to a simple dictionary type
        this->ConvertToSimpleDictionaryType(instance, GetPathLength());
    }

    void PathTypeHandlerBase::SetAllPropertiesToUndefined(DynamicObject* instance, bool invalidateFixedFields)
    {
        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.
        int propertyCount = GetPathLength();

        if (invalidateFixedFields)
        {
            Js::ScriptContext* scriptContext = instance->GetScriptContext();
            for (PropertyIndex propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
            {
                PropertyId propertyId = this->typePath->GetPropertyIdUnchecked(propertyIndex)->GetPropertyId();
                InvalidateFixedFieldAt(propertyId, propertyIndex, scriptContext);
            }
        }

        Js::RecyclableObject* undefined = instance->GetLibrary()->GetUndefined();
        for (PropertyIndex propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
        {
            SetSlotUnchecked(instance, propertyIndex, undefined);
        }

    }

    void PathTypeHandlerBase::MarshalAllPropertiesToScriptContext(DynamicObject* instance, ScriptContext* targetScriptContext, bool invalidateFixedFields)
    {
        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.
        int propertyCount = GetPathLength();

        if (invalidateFixedFields)
        {
            ScriptContext* scriptContext = instance->GetScriptContext();
            for (PropertyIndex propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
            {
                PropertyId propertyId = this->typePath->GetPropertyIdUnchecked(propertyIndex)->GetPropertyId();
                InvalidateFixedFieldAt(propertyId, propertyIndex, scriptContext);
            }
        }

        for (int slotIndex = 0; slotIndex < propertyCount; slotIndex++)
        {
            SetSlotUnchecked(instance, slotIndex, CrossSite::MarshalVar(targetScriptContext, GetSlot(instance, slotIndex)));
        }
    }

    BOOL PathTypeHandlerBase::AddProperty(DynamicObject * instance, PropertyId propertyId, Js::Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        if (attributes != PropertyDynamicTypeDefaults)
        {
            Assert(propertyId != Constants::NoProperty);
            PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
            return ConvertToSimpleDictionaryType(instance, GetPathLength() + 1)->AddProperty(instance, propertyRecord, value, attributes, info, flags, possibleSideEffects);
        }
        return AddPropertyInternal(instance, propertyId, value, info, flags, possibleSideEffects);
    }

    BOOL PathTypeHandlerBase::AddPropertyInternal(DynamicObject * instance, PropertyId propertyId, Js::Var value, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();

#if DBG
        uint32 indexVal;
        Assert(GetPropertyIndex(propertyId) == Constants::NoSlot);
        Assert(!scriptContext->IsNumericPropertyId(propertyId, &indexVal));
#endif

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);

        if (GetPathLength() >= TypePath::MaxPathTypeHandlerLength)
        {
#ifdef PROFILE_TYPES
            scriptContext->convertPathToDictionaryCount1++;
#endif
            return ConvertToSimpleDictionaryType(instance, GetPathLength() + 1)->AddProperty(instance, propertyRecord, value, PropertyDynamicTypeDefaults, info, PropertyOperation_None, possibleSideEffects);
        }

        PropertyIndex index;
        DynamicType* newType = PromoteType(instance, propertyRecord, &index);

        Assert(instance->GetTypeHandler()->IsPathTypeHandler());
        PathTypeHandlerBase* newTypeHandler = (PathTypeHandlerBase*)newType->GetTypeHandler();
        if (propertyId == PropertyIds::constructor)
        {
            newTypeHandler->isNotPathTypeHandlerOrHasUserDefinedCtor = true;
        }

        Assert(newType->GetIsShared() == newTypeHandler->GetIsShared());

        // Don't populate inline cache if this handler isn't yet shared.  If we did, a new instance could
        // reach this handler without us noticing and we could fail to release the old singleton instance, which may later
        // become collectible (not referenced by anything other than this handler), thus we would leak the old singleton instance.
        bool populateInlineCache = newTypeHandler->GetIsShared() ||
            ProcessFixedFieldChange(instance, propertyId, index, value, (flags & PropertyOperation_NonFixedValue) != 0, propertyRecord);

        SetSlotUnchecked(instance, index, value);

        if (populateInlineCache)
        {
            Assert((instance->GetDynamicType()->GetIsShared()) || (FixPropsOnPathTypes() && instance->GetDynamicType()->GetTypeHandler()->GetIsOrMayBecomeShared()));
            // Can't assert this.  With NewScObject we can jump to the type handler at the tip (where the singleton is),
            // even though we haven't yet initialized the properties all the way to the tip, and we don't want to kill
            // the singleton in that case yet.  It's basically a transient inconsistent state, but we have to live with it.
            // Assert(!instance->GetTypeHandler()->HasSingletonInstance());
            PropertyValueInfo::Set(info, instance, index);
        }
        else
        {
            PropertyValueInfo::SetNoCache(info, instance);
        }

        Assert(!IsolatePrototypes() || ((this->GetFlags() & IsPrototypeFlag) == 0));
        if (this->GetFlags() & IsPrototypeFlag)
        {
            scriptContext->InvalidateProtoCaches(propertyId);
        }
        SetPropertyUpdateSideEffect(instance, propertyId, value, possibleSideEffects);
        return true;
    }

    DynamicTypeHandler* PathTypeHandlerBase::ConvertToTypeWithItemAttributes(DynamicObject* instance)
    {
        return JavascriptArray::Is(instance) ?
            ConvertToES5ArrayType(instance) : ConvertToDictionaryType(instance);
    }

    void PathTypeHandlerBase::ShrinkSlotAndInlineSlotCapacity()
    {
        if (!GetIsInlineSlotCapacityLocked())
        {
            PathTypeHandlerBase * rootTypeHandler = GetRootPathTypeHandler();

            bool shrunk = false;
            uint16 maxPathLength = 0;
            if (rootTypeHandler->GetMaxPathLength(&maxPathLength))
            {
                uint16 newInlineSlotCapacity =
                    IsObjectHeaderInlinedTypeHandler()
                        ? RoundUpObjectHeaderInlinedInlineSlotCapacity(maxPathLength)
                        : RoundUpInlineSlotCapacity(maxPathLength);
                if (newInlineSlotCapacity < GetInlineSlotCapacity())
                {
                    rootTypeHandler->ShrinkSlotAndInlineSlotCapacity(newInlineSlotCapacity);
                    shrunk = true;
                }
            }

            if (!shrunk)
            {
                rootTypeHandler->LockInlineSlotCapacity();
            }
        }

#if DBG
        PathTypeHandlerBase * rootTypeHandler = GetRootPathTypeHandler();
        rootTypeHandler->VerifyInlineSlotCapacityIsLocked();
#endif
    }

    void PathTypeHandlerBase::EnsureInlineSlotCapacityIsLocked()
    {
        EnsureInlineSlotCapacityIsLocked(true);
#if DBG
        VerifyInlineSlotCapacityIsLocked();
#endif
    }

    void PathTypeHandlerBase::VerifyInlineSlotCapacityIsLocked()
    {
        VerifyInlineSlotCapacityIsLocked(true);
    }

    SimplePathTypeHandler *PathTypeHandlerBase::DeoptimizeObjectHeaderInlining(JavascriptLibrary *const library)
    {
        Assert(IsObjectHeaderInlinedTypeHandler());

        // Clone the type Path here to evolve separately
        uint16 pathLength = typePath->pathLength;
        TypePath * clonedPath = TypePath::New(library->GetRecycler(), pathLength);

        for (PropertyIndex i = 0; i < pathLength; i++)
        {
            clonedPath->assignments[i] = typePath->assignments[i];
            clonedPath->AddInternal(clonedPath->assignments[i]);
        }

        // We don't copy the fixed fields, as we will be sharing this type anyways later and the fixed fields vector has to be invalidated.
        SimplePathTypeHandler *const clonedTypeHandler =
            SimplePathTypeHandler::New(
                library->GetScriptContext(),
                clonedPath,
                GetPathLength(),
                static_cast<PropertyIndex>(GetSlotCapacity()),
                GetInlineSlotCapacity() - GetObjectHeaderInlinableSlotCapacity(),
                sizeof(DynamicObject),
                false,
                false);
        clonedTypeHandler->SetMayBecomeShared();
        return clonedTypeHandler;
    }

    void PathTypeHandlerBase::SetPrototype(DynamicObject* instance, RecyclableObject* newPrototype)
    {
        ConvertToSimpleDictionaryType(instance, GetPathLength())->SetPrototype(instance, newPrototype);
    }

    void PathTypeHandlerBase::SetIsPrototype(DynamicObject* instance)
    {
        // Don't return if IsPrototypeFlag is set, because we may still need to do a type transition and
        // set fixed bits.  If this handler is shared, this instance may not even be a prototype yet.
        // In this case we may need to convert to a non-shared type handler.
        if (!ChangeTypeOnProto() && !(GetIsOrMayBecomeShared() && IsolatePrototypes()))
        {
            SetFlags(IsPrototypeFlag);
            return;
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            DynamicType* oldType = instance->GetDynamicType();
            RecyclerWeakReference<DynamicObject>* oldSingletonInstance = GetSingletonInstance();
#endif

        if ((GetIsOrMayBecomeShared() && IsolatePrototypes()))
        {
            // The type coming in may not be shared or even locked (i.e. might have been created via DynamicObject::ChangeType()).
            // In that case the type handler change below won't change the type on the object, so we have to force it.

            DynamicType* oldType = instance->GetDynamicType();
            ConvertToSimpleDictionaryType(instance, GetPathLength());

            if (ChangeTypeOnProto() && instance->GetDynamicType() == oldType)
            {
                instance->ChangeType();
            }
        }
        else
        {

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            DynamicType* oldType = instance->GetDynamicType();
            RecyclerWeakReference<DynamicObject>* oldSingletonInstance = GetSingletonInstance();
            TraceFixedFieldsBeforeSetIsProto(instance, this, oldType, oldSingletonInstance);
#endif

            if (ChangeTypeOnProto())
            {
                // If this handler is shared and we don't isolate prototypes, it's possible that the handler has
                // the prototype flag, but this instance may not yet be a prototype and may not have taken
                // the required type transition.  It would be nice to have a reliable flag on the object
                // indicating whether it's a prototype to avoid multiple type transitions if the same object
                // with shared type handler is used as prototype multiple times.
                if (((GetFlags() & IsPrototypeFlag) == 0) || (GetIsShared() && !IsolatePrototypes()))
                {
                    // We're about to split out the type.  If the original type was shared the handler better be shared as well.
                    // Otherwise, the handler would lose track of being shared between different types and instances.
                    Assert(!instance->HasSharedType() || instance->GetDynamicType()->GetTypeHandler()->GetIsShared());

                    instance->ChangeType();
                    Assert(!instance->HasLockedType() && !instance->HasSharedType());
                }
            }
        }

        DynamicTypeHandler* typeHandler = GetCurrentTypeHandler(instance);
        if (typeHandler != this)
        {
            typeHandler->SetIsPrototype(instance);
        }
        else
        {
            SetFlags(IsPrototypeFlag);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            TraceFixedFieldsAfterSetIsProto(instance, this, typeHandler, oldType, instance->GetDynamicType(), oldSingletonInstance);
#endif

        }
    }

    bool PathTypeHandlerBase::HasSingletonInstance() const
    {
        Assert(HasSingletonInstanceOnlyIfNeeded());
        if (!FixPropsOnPathTypes())
        {
            return false;
        }

        return this->typePath->HasSingletonInstance() && GetPathLength() >= this->typePath->GetMaxInitializedLength();
    }

    void PathTypeHandlerBase::DoShareTypeHandler(ScriptContext* scriptContext)
    {
        DoShareTypeHandlerInternal<true>(scriptContext);
    }

    template <bool invalidateFixedFields>
    void PathTypeHandlerBase::DoShareTypeHandlerInternal(ScriptContext* scriptContext)
    {
        Assert((GetFlags() & (IsLockedFlag | MayBecomeSharedFlag | IsSharedFlag)) == (IsLockedFlag | MayBecomeSharedFlag));
        Assert(!IsolatePrototypes() || !GetIsOrMayBecomeShared() || !GetIsPrototype());

#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
        // If this handler is becoming shared we need to remove the singleton instance (so that it can be collected
        // if no longer referenced by anything else) and invalidate any fixed fields.
        if (FixPropsOnPathTypes())
        {
            if (invalidateFixedFields)
            {
                if (this->typePath->GetMaxInitializedLength() < GetPathLength())
                {
                    this->typePath->SetMaxInitializedLength(GetPathLength());
                }
                for (PropertyIndex index = 0; index < this->GetPathLength(); index++)
                {
                    InvalidateFixedFieldAt(this->typePath->GetPropertyIdUnchecked(index)->GetPropertyId(), index, scriptContext);
                }
            }

            Assert(HasOnlyInitializedNonFixedProperties());
            Assert(HasSingletonInstanceOnlyIfNeeded());
            if (HasSingletonInstance())
            {
                this->typePath->ClearSingletonInstance();
            }
        }
#endif
    }

    void PathTypeHandlerBase::InvalidateFixedFieldAt(Js::PropertyId propertyId, Js::PropertyIndex index, ScriptContext* scriptContext)
    {
        if (!FixPropsOnPathTypes())
        {
            return;
        }

        // We are adding a new value where some other instance already has an existing value.  If this is a fixed
        // field we must clear the bit. If the value was hard coded in the JIT-ed code, we must invalidate the guards.
        if (this->typePath->GetIsUsedFixedFieldAt(index, GetPathLength()))
        {
            // We may be a second instance chasing the singleton and invalidating fixed fields along the way.
            // Assert(newTypeHandler->typePath->GetSingletonInstance() == instance);

            // Invalidate any JIT-ed code that hard coded this method. No need to invalidate store field
            // inline caches (which might quietly overwrite this fixed fields, because they have never been populated.
#if ENABLE_NATIVE_CODEGEN
            scriptContext->GetThreadContext()->InvalidatePropertyGuards(propertyId);
#endif
        }

        // If we're overwriting an existing value of this property, we don't consider the new one fixed.
        // This also means that it's ok to populate the inline caches for this property from now on.
        this->typePath->ClearIsFixedFieldAt(index, GetPathLength());
    }

    void PathTypeHandlerBase::AddBlankFieldAt(Js::PropertyId propertyId, Js::PropertyIndex index, ScriptContext* scriptContext)
    {
        if (!FixPropsOnPathTypes())
        {
            return;
        }

        if (index >= this->typePath->GetMaxInitializedLength())
        {
            // We are a adding a property where no instance property has been set before.  We rely on properties being
            // added in order of indexes to be sure that we don't leave any uninitialized properties interspersed with
            // initialized ones, which could lead to incorrect behavior.  See comment in TypePath::Branch.
            AssertMsg(index == this->typePath->GetMaxInitializedLength(), "Adding properties out of order?");

            this->typePath->AddBlankFieldAt(index, GetPathLength());
        }
        else
        {
            InvalidateFixedFieldAt(propertyId, index, scriptContext);

            // We have now reached the most advanced instance along this path.  If this instance is not the singleton instance,
            // then the former singleton instance (if any) is no longer a singleton.  This instance could be the singleton
            // instance, if we just happen to set (overwrite) its last property.
            if (index + 1 == this->typePath->GetMaxInitializedLength())
            {
                // If we cleared the singleton instance while some fields remained fixed, the instance would
                // be collectible, and yet some code would expect to see values and call methods on it. We rely on the
                // fact that we always add properties to (pre-initialized) type handlers in the order they appear
                // on the type path.  By the time we reach the singleton instance, all fixed fields will have been invalidated.
                // Otherwise, some fields could remain fixed (or even uninitialized) and we would have to spin off a loop here
                // to invalidate any remaining fixed fields
                Assert(HasSingletonInstanceOnlyIfNeeded());
                this->typePath->ClearSingletonInstance();
            }

        }
    }

    bool PathTypeHandlerBase::ProcessFixedFieldChange(DynamicObject* instance, PropertyId propertyId, PropertyIndex slotIndex, Var value, bool isNonFixed,const PropertyRecord * propertyRecord)
    {
        Assert(!instance->GetTypeHandler()->GetIsShared());
        // We don't want fixed properties on external objects, either external properties or expando properties.
        // See DynamicObject::ResetObject for more information.
        Assert(!instance->IsExternal() || isNonFixed);

        if (!FixPropsOnPathTypes())
        {
            return true;
        }

        bool populateInlineCache = true;

        PathTypeHandler* newTypeHandler = (PathTypeHandler*)instance->GetTypeHandler();

        if (slotIndex >= newTypeHandler->typePath->GetMaxInitializedLength())
        {
            // We are a adding a property where no instance property has been set before.  We rely on properties being
            // added in order of indexes to be sure that we don't leave any uninitialized properties interspersed with
            // initialized ones, which could lead to incorrect behavior.  See comment in TypePath::Branch.
            AssertMsg(slotIndex == newTypeHandler->typePath->GetMaxInitializedLength(), "Adding properties out of order?");

            // Consider: It would be nice to assert the slot is actually null.  However, we sometimes pre-initialize to
            // undefined or even some other special illegal value (for let or const, currently == null)
            // Assert(instance->GetSlot(index) == nullptr);

            if (ShouldFixAnyProperties() && CanBeSingletonInstance(instance))
            {
                bool markAsFixed = !isNonFixed && !IsInternalPropertyId(propertyId) &&
                    (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() || ShouldFixAccessorProperties() :
                                    (ShouldFixDataProperties() && CheckHeuristicsForFixedDataProps(instance, propertyRecord, propertyId, value)));

                // Mark the newly added field as fixed and prevent population of inline caches.

                newTypeHandler->typePath->AddSingletonInstanceFieldAt(instance, slotIndex, markAsFixed, newTypeHandler->GetPathLength());
            }
            else
            {
                newTypeHandler->typePath->AddSingletonInstanceFieldAt(slotIndex, newTypeHandler->GetPathLength());
            }

            populateInlineCache = false;
        }
        else
        {
            newTypeHandler->InvalidateFixedFieldAt(propertyId, slotIndex, instance->GetScriptContext());

            // We have now reached the most advanced instance along this path.  If this instance is not the singleton instance,
            // then the former singleton instance (if any) is no longer a singleton.  This instance could be the singleton
            // instance, if we just happen to set (overwrite) its last property.
            if (slotIndex + 1 == newTypeHandler->typePath->GetMaxInitializedLength())
            {
                // If we cleared the singleton instance while some fields remained fixed, the instance would
                // be collectible, and yet some code would expect to see values and call methods on it. We rely on the
                // fact that we always add properties to (pre-initialized) type handlers in the order they appear
                // on the type path.  By the time we reach the singleton instance, all fixed fields will have been invalidated.
                // Otherwise, some fields could remain fixed (or even uninitialized) and we would have to spin off a loop here
                // to invalidate any remaining fixed fields
                auto singletonWeakRef = newTypeHandler->typePath->GetSingletonInstance();
                if (singletonWeakRef != nullptr && instance != singletonWeakRef->Get())
                {
                    Assert(newTypeHandler->HasSingletonInstanceOnlyIfNeeded());
                    newTypeHandler->typePath->ClearSingletonInstance();
                }
            }
        }

        // If we branched and this is the singleton instance, we need to remove it from this type handler.  The only time
        // this can happen is when another not fully initialized instance is ahead of this one on the current path.
        auto singletonWeakRef = this->typePath->GetSingletonInstance();
        if (newTypeHandler->typePath != this->typePath && singletonWeakRef != nullptr && singletonWeakRef->Get() == instance)
        {
            // If this is the singleton instance, there shouldn't be any other initialized instance ahead of it on the old path.
            Assert(GetPathLength() >= this->typePath->GetMaxInitializedLength());
            Assert(HasSingletonInstanceOnlyIfNeeded());
            this->typePath->ClearSingletonInstance();
        }

        return populateInlineCache;
    }

    bool PathTypeHandlerBase::TryUseFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext)
    {
        bool result = TryGetFixedProperty<false, true>(propertyRecord, pProperty, propertyType, requestContext);
        TraceUseFixedProperty(propertyRecord, pProperty, result, L"PathTypeHandler", requestContext);
        return result;
    }

    bool PathTypeHandlerBase::TryUseFixedAccessor(PropertyRecord const * propertyRecord, Var * pAccessor, FixedPropertyKind propertyType, bool getter, ScriptContext * requestContext)
    {
        if (PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase) ||
            PHASE_VERBOSE_TRACE1(Js::UseFixedDataPropsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::UseFixedDataPropsPhase))
        {
            Output::Print(L"FixedFields: attempt to use fixed accessor %s from PathTypeHandler returned false.\n", propertyRecord->GetBuffer());
            if (this->HasSingletonInstance() && this->GetSingletonInstance()->Get()->GetScriptContext() != requestContext)
            {
                Output::Print(L"FixedFields: Cross Site Script Context is used for property %s. \n", propertyRecord->GetBuffer());
            }
            Output::Flush();
        }
        return false;
    }

#if DBG
    bool PathTypeHandlerBase::CanStorePropertyValueDirectly(const DynamicObject* instance, PropertyId propertyId, bool allowLetConst)
    {
        Assert(!allowLetConst);
        // We pass Constants::NoProperty for ActivationObjects for functions with same named formals, but we don't
        // use PathTypeHandlers for those.
        Assert(propertyId != Constants::NoProperty);
        Js::PropertyIndex index = GetPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {
            if (FixPropsOnPathTypes())
            {
                return index < this->typePath->GetMaxInitializedLength() && !this->typePath->GetIsFixedFieldAt(index, this->GetPathLength());
            }
            else
            {
                return true;
            }
        }
        else
        {
            AssertMsg(false, "Asking about a property this type handler doesn't know about?");
            return false;
        }
    }

    bool PathTypeHandlerBase::HasOnlyInitializedNonFixedProperties()
    {

#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
      if (this->typePath->GetMaxInitializedLength() < GetPathLength())
      {
          return false;
      }

      for (PropertyIndex index = 0; index < this->GetPathLength(); index++)
      {
          if (this->typePath->GetIsFixedFieldAt(index, this->GetPathLength()))
          {
              return false;
          }
      }
#endif

      return true;
    }

    bool PathTypeHandlerBase::CheckFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, ScriptContext * requestContext)
    {
        return TryGetFixedProperty<true, false>(propertyRecord, pProperty, (Js::FixedPropertyKind)(Js::FixedPropertyKind::FixedMethodProperty | Js::FixedPropertyKind::FixedDataProperty), requestContext);
    }

    bool PathTypeHandlerBase::HasAnyFixedProperties() const
    {
        int pathLength = GetPathLength();
        for (PropertyIndex i = 0; i < pathLength; i++)
        {
            if (this->typePath->GetIsFixedFieldAt(i, pathLength))
            {
                return true;
            }
        }
        return false;
    }
#endif

    template <bool allowNonExistent, bool markAsUsed>
    bool PathTypeHandlerBase::TryGetFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, Js::FixedPropertyKind propertyType, ScriptContext * requestContext)
    {
        if (!FixPropsOnPathTypes())
        {
            return false;
        }

        PropertyIndex index = this->typePath->Lookup(propertyRecord->GetPropertyId(), GetPathLength());
        if (index == Constants::NoSlot)
        {
            AssertMsg(allowNonExistent, "Trying to get a fixed function instance for a non-existent property?");
            return false;
        }

        Var value = this->typePath->GetSingletonFixedFieldAt(index, GetPathLength(), requestContext);
        if (value && ((IsFixedMethodProperty(propertyType) && JavascriptFunction::Is(value)) || IsFixedDataProperty(propertyType)))
        {
            *pProperty = value;
            if (markAsUsed)
            {
                this->typePath->SetIsUsedFixedFieldAt(index, GetPathLength());
            }
            return true;
        }
        else
        {
            return false;
        }
    }

    PathTypeHandlerBase* PathTypeHandlerBase::GetRootPathTypeHandler()
    {
        PathTypeHandlerBase* rootTypeHandler = this;
        while (rootTypeHandler->predecessorType != nullptr)
        {
            rootTypeHandler = PathTypeHandlerBase::FromTypeHandler(rootTypeHandler->predecessorType->GetTypeHandler());
        }
        Assert(rootTypeHandler->predecessorType == nullptr);
        return rootTypeHandler;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void PathTypeHandlerBase::DumpFixedFields() const {
        if (FixPropsOnPathTypes())
        {
            for (PropertyIndex i = 0; i < GetPathLength(); i++)
            {
                Output::Print(L" %s %d%d%d,", typePath->GetPropertyId(i)->GetBuffer(),
                    i < this->typePath->GetMaxInitializedLength() ? 1 : 0,
                    this->typePath->GetIsFixedFieldAt(i, GetPathLength()) ? 1 : 0,
                    this->typePath->GetIsUsedFixedFieldAt(i, GetPathLength()) ? 1 : 0);
            }
        }
        else
        {
            for (PropertyIndex i = 0; i < GetPathLength(); i++)
            {
                Output::Print(L" %s %d%d%d,", typePath->GetPropertyId(i)->GetBuffer(), 1, 0, 0);
            }
        }
    }

    void PathTypeHandlerBase::TraceFixedFieldsBeforeTypeHandlerChange(
        const wchar_t* conversionName, const wchar_t* oldTypeHandlerName, const wchar_t* newTypeHandlerName,
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler,
        DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
    {
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {
            Output::Print(L"FixedFields: %s 0x%p from %s to %s:\n", conversionName, instance, oldTypeHandlerName, newTypeHandlerName);
            Output::Print(L"   before: type = 0x%p, type handler = 0x%p, old singleton = 0x%p(0x%p)\n",
                oldType, oldTypeHandler, oldSingletonInstanceBefore, oldSingletonInstanceBefore != nullptr ? oldSingletonInstanceBefore->Get() : nullptr);
            Output::Print(L"   fixed fields:");
            oldTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
        }
        if (PHASE_VERBOSE_TESTTRACE1(FixMethodPropsPhase))
        {
            Output::Print(L"FixedFields: %s instance from %s to %s:\n", conversionName, oldTypeHandlerName, newTypeHandlerName);
            Output::Print(L"   old singleton before %s null \n", oldSingletonInstanceBefore == nullptr ? L"==" : L"!=");
            Output::Print(L"   fixed fields before:");
            oldTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
        }
    }

    void PathTypeHandlerBase::TraceFixedFieldsAfterTypeHandlerChange(
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicTypeHandler* newTypeHandler,
        DynamicType* oldType, DynamicType* newType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
    {
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {
            RecyclerWeakReference<DynamicObject>* oldSingletonInstanceAfter = oldTypeHandler->GetSingletonInstance();
            RecyclerWeakReference<DynamicObject>* newSingletonInstanceAfter = newTypeHandler->GetSingletonInstance();
            Output::Print(L"   after: type = 0x%p, type handler = 0x%p, old singleton = 0x%p(0x%p), new singleton = 0x%p(0x%p)\n",
                newType, newTypeHandler, oldSingletonInstanceAfter, oldSingletonInstanceAfter != nullptr ? oldSingletonInstanceAfter->Get() : nullptr,
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
                oldType != newType ? L"changed" : L"unchanged",
                oldTypeHandler->GetSingletonInstance() == nullptr ? L"==" : L"!=",
                oldSingletonInstanceBefore != oldTypeHandler->GetSingletonInstance() ? L"changed" : L"unchanged",
                newTypeHandler->GetSingletonInstance() == nullptr ? L"==" : L"!=");
            Output::Print(L"   fixed fields after:");
            newTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
            Output::Flush();
        }
    }

    void PathTypeHandlerBase::TraceFixedFieldsBeforeSetIsProto(
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
    {
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {
            Output::Print(L"FixedFields: PathTypeHandler::SetIsPrototype(0x%p):\n", instance);
            Output::Print(L"   before: type = 0x%p, type handler = 0x%p, old singleton = 0x%p(0x%p)\n",
                oldType, oldTypeHandler, oldSingletonInstanceBefore, oldSingletonInstanceBefore != nullptr ? oldSingletonInstanceBefore->Get() : nullptr);
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

    void PathTypeHandlerBase::TraceFixedFieldsAfterSetIsProto(
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicTypeHandler* newTypeHandler,
        DynamicType* oldType, DynamicType* newType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
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
                oldType != newType ? L"changed" : L"unchanged",
                oldSingletonInstanceBefore == nullptr ? L"==" : L"!=",
                oldSingletonInstanceBefore != oldTypeHandler->GetSingletonInstance() ? L"changed" : L"unchanged");
            Output::Print(L"   fixed fields after:");
            newTypeHandler->DumpFixedFields();
            Output::Print(L"\n");
            Output::Flush();
        }
    }
#endif

    SimplePathTypeHandler * SimplePathTypeHandler::New(ScriptContext * scriptContext, TypePath* typePath, uint16 pathLength, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked, bool isShared, DynamicType* predecessorType)
    {
        return New(scriptContext, typePath, pathLength, max(pathLength, inlineSlotCapacity), inlineSlotCapacity, offsetOfInlineSlots, isLocked, isShared, predecessorType);
    }

    SimplePathTypeHandler * SimplePathTypeHandler::New(ScriptContext * scriptContext, TypePath* typePath, uint16 pathLength, const PropertyIndex slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked, bool isShared, DynamicType* predecessorType)
    {
        Assert(typePath != nullptr);
#ifdef PROFILE_TYPES
        scriptContext->simplePathTypeHandlerCount++;
#endif
        return RecyclerNew(scriptContext->GetRecycler(), SimplePathTypeHandler, typePath, pathLength, slotCapacity, inlineSlotCapacity, offsetOfInlineSlots, isLocked, isShared, predecessorType);
    }

    SimplePathTypeHandler * SimplePathTypeHandler::New(ScriptContext * scriptContext, SimplePathTypeHandler * typeHandler, bool isLocked, bool isShared)
    {
        Assert(typeHandler != nullptr);
        return RecyclerNew(scriptContext->GetRecycler(), SimplePathTypeHandler, typeHandler->GetTypePath(), typeHandler->GetPathLength(), typeHandler->GetInlineSlotCapacity(), typeHandler->GetOffsetOfInlineSlots(), isLocked, isShared);
    }

    SimplePathTypeHandler::SimplePathTypeHandler(TypePath* typePath, uint16 pathLength, const PropertyIndex slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked, bool isShared, DynamicType* predecessorType) :
        PathTypeHandlerBase(typePath, pathLength, slotCapacity, inlineSlotCapacity, offsetOfInlineSlots, isLocked, isShared, predecessorType),
        successorPropertyRecord(nullptr),
        successorTypeWeakRef(nullptr)
    {
    }

    void SimplePathTypeHandler::ShrinkSlotAndInlineSlotCapacity(uint16 newInlineSlotCapacity)
    {
        Assert(!this->GetIsInlineSlotCapacityLocked());
        this->SetInlineSlotCapacity(newInlineSlotCapacity);
        this->SetSlotCapacity(newInlineSlotCapacity);
        this->SetIsInlineSlotCapacityLocked();
        if (this->successorPropertyRecord)
        {
            DynamicType * type = successorTypeWeakRef->Get();
            if (type)
            {
                PathTypeHandlerBase::FromTypeHandler(type->GetTypeHandler())->ShrinkSlotAndInlineSlotCapacity(newInlineSlotCapacity);
            }
        }
    }

    void SimplePathTypeHandler::LockInlineSlotCapacity()
    {
        Assert(!GetIsInlineSlotCapacityLocked());
        SetIsInlineSlotCapacityLocked();

        if (!successorPropertyRecord)
        {
            return;
        }

        DynamicType * type = successorTypeWeakRef->Get();
        if (type)
        {
            type->GetTypeHandler()->LockInlineSlotCapacity();
        }
    }

    void SimplePathTypeHandler::EnsureInlineSlotCapacityIsLocked(bool startFromRoot)
    {
        if (startFromRoot)
        {
            GetRootPathTypeHandler()->EnsureInlineSlotCapacityIsLocked(false);
            return;
        }

        Assert(!startFromRoot);

        if (!GetIsInlineSlotCapacityLocked())
        {
            SetIsInlineSlotCapacityLocked();

            if (successorPropertyRecord)
            {
                DynamicType * type = successorTypeWeakRef->Get();
                if (type)
                {
                    DynamicTypeHandler* successorTypeHandler = type->GetTypeHandler();
                    successorTypeHandler->IsPathTypeHandler() ?
                        PathTypeHandler::FromTypeHandler(successorTypeHandler)->EnsureInlineSlotCapacityIsLocked(false) :
                        successorTypeHandler->EnsureInlineSlotCapacityIsLocked();
                }
            }
        }
    }

    void SimplePathTypeHandler::VerifyInlineSlotCapacityIsLocked(bool startFromRoot)
    {
        if (startFromRoot)
        {
            GetRootPathTypeHandler()->VerifyInlineSlotCapacityIsLocked(false);
            return;
        }

        Assert(!startFromRoot);

        Assert(GetIsInlineSlotCapacityLocked());

        if (!successorPropertyRecord)
        {
            return;
        }

        DynamicType * type = successorTypeWeakRef->Get();
        if (type)
        {
            DynamicTypeHandler* successorTypeHandler = type->GetTypeHandler();
            successorTypeHandler->IsPathTypeHandler() ?
                PathTypeHandler::FromTypeHandler(successorTypeHandler)->VerifyInlineSlotCapacityIsLocked(false) :
                successorTypeHandler->VerifyInlineSlotCapacityIsLocked();
        }
    }

    bool SimplePathTypeHandler::GetMaxPathLength(uint16 * maxPathLength)
    {
        if (GetPathLength() > *maxPathLength)
        {
            *maxPathLength = GetPathLength();
        }

        if (!successorPropertyRecord)
        {
            return true;
        }

        DynamicType * type = successorTypeWeakRef->Get();
        if (type)
        {
            if (!type->GetTypeHandler()->IsPathTypeHandler())
            {
                return false;
            }
            if (!PathTypeHandlerBase::FromTypeHandler(type->GetTypeHandler())->GetMaxPathLength(maxPathLength))
            {
                return false;
            }
        }
        return true;
    }

    bool SimplePathTypeHandler::GetSuccessor(const PropertyRecord* propertyRecord, RecyclerWeakReference<DynamicType> ** typeWeakRef)
    {
        if (successorPropertyRecord != propertyRecord)
        {
            *typeWeakRef = nullptr;
            return false;
        }
        *typeWeakRef = successorTypeWeakRef;
        return true;
    }

    void SimplePathTypeHandler::SetSuccessor(DynamicType * type, const PropertyRecord* propertyRecord, RecyclerWeakReference<DynamicType> * typeWeakRef, ScriptContext * scriptContext)
    {
        if (!successorPropertyRecord || successorPropertyRecord == propertyRecord || !successorTypeWeakRef->Get())
        {
            successorPropertyRecord = propertyRecord;
            successorTypeWeakRef = typeWeakRef;
            return;
        }

        // This is an interesting transition from the fixed fields perspective.  If there are any other types using this type handler
        // (which can happen if we don't isolate prototypes but force type change on becoming proto), they will continue to do so. So
        // we will have two different type handlers at the exact same point in type path evolution sharing the same type path, and
        // consequently all fixed field info as well.  This is fine, because fixed field management is done at the type path level.
        PathTypeHandler * newTypeHandler = PathTypeHandler::New(scriptContext, GetTypePath(), GetPathLength(), static_cast<PropertyIndex>(GetSlotCapacity()), GetInlineSlotCapacity(), GetOffsetOfInlineSlots(), true, true, GetPredecessorType());
        newTypeHandler->SetSuccessor(type, this->successorPropertyRecord, this->successorTypeWeakRef, scriptContext);
        newTypeHandler->SetSuccessor(type, propertyRecord, typeWeakRef, scriptContext);
        newTypeHandler->SetFlags(IsPrototypeFlag, GetFlags());
        newTypeHandler->CopyPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection | PropertyTypesInlineSlotCapacityLocked, this->GetPropertyTypes());
        // We don't transfer any fixed field data because we assume the type path remains the same.
        Assert(newTypeHandler->GetTypePath() == this->GetTypePath());

        Assert(type->typeHandler == this);
        type->typeHandler = newTypeHandler;

#ifdef PROFILE_TYPES
        scriptContext->convertSimplePathToPathCount++;
#endif
    }

    PathTypeHandler * PathTypeHandler::New(ScriptContext * scriptContext, TypePath* typePath, uint16 pathLength, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked, bool isShared, DynamicType* predecessorType)
    {
        return New(scriptContext, typePath, pathLength, max(pathLength, inlineSlotCapacity), inlineSlotCapacity, offsetOfInlineSlots, isLocked, isShared, predecessorType);
    }

    PathTypeHandler * PathTypeHandler::New(ScriptContext * scriptContext, TypePath* typePath, uint16 pathLength, const PropertyIndex slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked, bool isShared, DynamicType* predecessorType)
    {
        Assert(typePath != nullptr);
#ifdef PROFILE_TYPES
        scriptContext->pathTypeHandlerCount++;
#endif
        return RecyclerNew(scriptContext->GetRecycler(), PathTypeHandler, typePath, pathLength, slotCapacity, inlineSlotCapacity, offsetOfInlineSlots, isLocked, isShared, predecessorType);
    }

    PathTypeHandler * PathTypeHandler::New(ScriptContext * scriptContext, PathTypeHandler * typeHandler, bool isLocked, bool isShared)
    {
        Assert(typeHandler != nullptr);
        return RecyclerNew(scriptContext->GetRecycler(), PathTypeHandler, typeHandler->GetTypePath(), typeHandler->GetPathLength(), static_cast<PropertyIndex>(typeHandler->GetSlotCapacity()), typeHandler->GetInlineSlotCapacity(), typeHandler->GetOffsetOfInlineSlots(), isLocked, isShared);
    }

    PathTypeHandler::PathTypeHandler(TypePath* typePath, uint16 pathLength, const PropertyIndex slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, bool isLocked, bool isShared, DynamicType* predecessorType) :
        PathTypeHandlerBase(typePath, pathLength, slotCapacity, inlineSlotCapacity, offsetOfInlineSlots, isLocked, isShared, predecessorType),
        propertySuccessors(nullptr)
    {
    }

    void PathTypeHandler::ShrinkSlotAndInlineSlotCapacity(uint16 newInlineSlotCapacity)
    {
        Assert(!this->GetIsInlineSlotCapacityLocked());
        this->SetInlineSlotCapacity(newInlineSlotCapacity);
        // Slot capacity should also be shrunk when the inlineSlotCapacity is shrunk.
        this->SetSlotCapacity(newInlineSlotCapacity);
        this->SetIsInlineSlotCapacityLocked();
        if (this->propertySuccessors)
        {
            this->propertySuccessors->Map([newInlineSlotCapacity](PropertyId, RecyclerWeakReference<DynamicType> * typeWeakReference)
            {
                DynamicType * type = typeWeakReference->Get();
                if (type)
                {
                    PathTypeHandlerBase::FromTypeHandler(type->GetTypeHandler())->ShrinkSlotAndInlineSlotCapacity(newInlineSlotCapacity);
                }
            });
        }
    }

    void PathTypeHandler::LockInlineSlotCapacity()
    {
        Assert(!GetIsInlineSlotCapacityLocked());
        SetIsInlineSlotCapacityLocked();

        if (!propertySuccessors || propertySuccessors->Count() == 0)
        {
            return;
        }

        this->propertySuccessors->Map([](const PropertyId, RecyclerWeakReference<DynamicType>* typeWeakReference)
        {
            DynamicType * type = typeWeakReference->Get();
            if (!type)
            {
                return;
            }

            type->GetTypeHandler()->LockInlineSlotCapacity();
        });
    }

    void PathTypeHandler::EnsureInlineSlotCapacityIsLocked(bool startFromRoot)
    {
        if (startFromRoot)
        {
            GetRootPathTypeHandler()->EnsureInlineSlotCapacityIsLocked(false);
            return;
        }

        Assert(!startFromRoot);

        if (!GetIsInlineSlotCapacityLocked())
        {
            SetIsInlineSlotCapacityLocked();

            if (propertySuccessors && propertySuccessors->Count() > 0)
            {
                this->propertySuccessors->Map([](const PropertyId, RecyclerWeakReference<DynamicType> * typeWeakReference)
                {
                    DynamicType * type = typeWeakReference->Get();
                    if (!type)
                    {
                        return;
                    }

                    DynamicTypeHandler* successorTypeHandler = type->GetTypeHandler();
                    successorTypeHandler->IsPathTypeHandler() ?
                        PathTypeHandler::FromTypeHandler(successorTypeHandler)->EnsureInlineSlotCapacityIsLocked(false) :
                        successorTypeHandler->EnsureInlineSlotCapacityIsLocked();

                });
            }
        }
    }

    void PathTypeHandler::VerifyInlineSlotCapacityIsLocked(bool startFromRoot)
    {
        if (startFromRoot)
        {
            GetRootPathTypeHandler()->VerifyInlineSlotCapacityIsLocked(false);
            return;
        }

        Assert(!startFromRoot);

        Assert(GetIsInlineSlotCapacityLocked());

        if (!propertySuccessors || propertySuccessors->Count() == 0)
        {
            return;
        }

        this->propertySuccessors->Map([](const PropertyId, RecyclerWeakReference<DynamicType> * typeWeakReference)
        {
            DynamicType * type = typeWeakReference->Get();
            if (!type)
            {
                return;
            }

            DynamicTypeHandler* successorTypeHandler = type->GetTypeHandler();
            successorTypeHandler->IsPathTypeHandler() ?
                PathTypeHandler::FromTypeHandler(successorTypeHandler)->VerifyInlineSlotCapacityIsLocked(false) :
                successorTypeHandler->VerifyInlineSlotCapacityIsLocked();
        });
    }

    bool PathTypeHandler::GetMaxPathLength(uint16 * maxPathLength)
    {
        if (GetPropertyCount() > *maxPathLength)
        {
            *maxPathLength = GetPathLength();
        }

        if (!propertySuccessors || propertySuccessors->Count() == 0)
        {
            return true;
        }

        bool result = true;
        this->propertySuccessors->MapUntil([&result, maxPathLength](PropertyId, RecyclerWeakReference<DynamicType> * typeWeakReference) -> bool
        {
            DynamicType * type = typeWeakReference->Get();
            if (!type)
            {
                return false;
            }
            if (!type->GetTypeHandler()->IsPathTypeHandler())
            {
                result = false;
                return true;
            }
            if (!PathTypeHandlerBase::FromTypeHandler(type->GetTypeHandler())->GetMaxPathLength(maxPathLength))
            {
                result = false;
                return true;
            }

            return false;
        });

        return result;
    }

    bool PathTypeHandler::GetSuccessor(const PropertyRecord* propertyRecord, RecyclerWeakReference<DynamicType> ** typeWeakRef)
    {
        if (!propertySuccessors || !propertySuccessors->TryGetValue(propertyRecord->GetPropertyId(), typeWeakRef))
        {
            *typeWeakRef = nullptr;
            return false;
        }
        return true;
    }

    void PathTypeHandler::SetSuccessor(DynamicType * type, const PropertyRecord* propertyRecord, RecyclerWeakReference<DynamicType> * typeWeakRef, ScriptContext * scriptContext)
    {
        if (!propertySuccessors)
        {
            Recycler * recycler = scriptContext->GetRecycler();
            propertySuccessors = RecyclerNew(recycler, PropertySuccessorsMap, recycler, 3);
        }
        propertySuccessors->Item(propertyRecord->GetPropertyId(), typeWeakRef);
    }
}

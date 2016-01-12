//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

#include "Types\NullTypeHandler.h"
#include "Types\SimpleTypeHandler.h"

namespace Js
{
    int NullTypeHandlerBase::GetPropertyCount()
    {
        return 0;
    }


    PropertyId NullTypeHandlerBase::GetPropertyId(ScriptContext* scriptContext, PropertyIndex index)
    {
        return Constants::NoProperty;
    }

    PropertyId NullTypeHandlerBase::GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index)
    {
        return Constants::NoProperty;
    }


    BOOL NullTypeHandlerBase::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyString, PropertyId* propertyId,
        PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, bool requireEnumerable, bool enumSymbols)
    {
        Assert(propertyString);
        Assert(propertyId);
        Assert(type);
        return FALSE;
    }


    PropertyIndex NullTypeHandlerBase::GetPropertyIndex(PropertyRecord const* propertyRecord)
    {
        return Constants::NoSlot;
    }

    bool NullTypeHandlerBase::GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info)
    {
        info.slotIndex = Constants::NoSlot;
        info.isAuxSlot = false;
        info.isWritable = false;
        return false;
    }

    bool NullTypeHandlerBase::IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex)
    {
        uint propertyCount = record.propertyCount;
        EquivalentPropertyEntry* properties = record.properties;
        for (uint pi = 0; pi < propertyCount; pi++)
        {
            const EquivalentPropertyEntry* refInfo = &properties[pi];
            if (!this->NullTypeHandlerBase::IsObjTypeSpecEquivalent(type, refInfo))
            {
                return false;
            }
        }
        return true;
    }

    bool NullTypeHandlerBase::IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry *entry)
    {
        return entry->slotIndex == Constants::NoSlot && !entry->mustBeWritable;
    }

    BOOL NullTypeHandlerBase::HasProperty(DynamicObject* instance, PropertyId propertyId, __out_opt bool *noRedecl)
    {
        // Check numeric propertyId only if objectArray is available
        uint32 indexVal;
        ScriptContext* scriptContext = instance->GetScriptContext();

        if (noRedecl != nullptr)
        {
            *noRedecl = false;
        }

        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return DynamicTypeHandler::HasItem(instance, indexVal);
        }

        return false;
    }


    BOOL NullTypeHandlerBase::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {
        PropertyRecord const* propertyRecord;
        instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return NullTypeHandlerBase::HasProperty(instance, propertyRecord->GetPropertyId());
    }


    BOOL NullTypeHandlerBase::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        // Check numeric propertyId only if objectArray is available
        uint32 indexVal;
        ScriptContext* scriptContext = instance->GetScriptContext();
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return DynamicTypeHandler::GetItem(instance, originalInstance, indexVal, value, requestContext);
        }

        return false;
    }


    BOOL NullTypeHandlerBase::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        PropertyRecord const* propertyRecord;
        instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return NullTypeHandlerBase::GetProperty(instance, originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext);
    }


    BOOL NullTypeHandlerBase::SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        return ConvertToSimpleType(instance)->SetProperty(instance, propertyId, value, flags, info);
    }


    BOOL NullTypeHandlerBase::SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        return ConvertToSimpleType(instance)->SetProperty(instance, propertyNameString, value, flags, info);
    }

    BOOL NullTypeHandlerBase::AddProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        if (this->isPrototype && (ChangeTypeOnProto() || (GetIsShared() && IsolatePrototypes())))
        {
            ScriptContext* scriptContext = instance->GetScriptContext();
            return ConvertToSimpleDictionaryType(instance)->AddProperty(instance, scriptContext->GetPropertyName(propertyId), value, attributes, info, flags, possibleSideEffects);
        }
        else
        {
            return ConvertToSimpleType(instance)->AddProperty(instance, propertyId, value, attributes, info, flags, possibleSideEffects);
        }
    }

    BOOL NullTypeHandlerBase::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags flags)
    {
        // Check numeric propertyId only if objectArray is available
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 indexVal;
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {
            return DynamicTypeHandler::DeleteItem(instance, indexVal, flags);
        }

        return true;
    }


    BOOL NullTypeHandlerBase::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {
        return true;
    }


    BOOL NullTypeHandlerBase::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {
        return true;
    }


    BOOL NullTypeHandlerBase::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {
        return true;
    }


    BOOL NullTypeHandlerBase::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        return false;
    }


    BOOL NullTypeHandlerBase::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        return false;
    }


    BOOL NullTypeHandlerBase::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {
        return false;
    }


    BOOL NullTypeHandlerBase::SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {
        return ConvertToDictionaryType(instance)->SetAccessors(instance, propertyId, getter, setter, flags);
    }


    BOOL NullTypeHandlerBase::PreventExtensions(DynamicObject* instance)
    {
        return ConvertToDictionaryType(instance)->PreventExtensions(instance);
    }


    BOOL NullTypeHandlerBase::Seal(DynamicObject* instance)
    {
        return ConvertToDictionaryType(instance)->Seal(instance);
    }


    BOOL NullTypeHandlerBase::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {
        return ConvertToDictionaryType(instance)->Freeze(instance, true);
    }

    template <typename T>
    T* NullTypeHandlerBase::ConvertToTypeHandler(DynamicObject* instance)
    {
        ScriptContext* scriptContext = instance->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();

        T * newTypeHandler = RecyclerNew(recycler, T, recycler);
        Assert((newTypeHandler->GetFlags() & IsPrototypeFlag) == 0);
        // EnsureSlots before updating the type handler and instance, as EnsureSlots allocates and may throw.
        instance->EnsureSlots(0, newTypeHandler->GetSlotCapacity(), scriptContext, newTypeHandler);
        Assert(((this->GetFlags() & IsPrototypeFlag) != 0) == this->isPrototype);
        newTypeHandler->SetFlags(IsPrototypeFlag, this->GetFlags());
        newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection | PropertyTypesInlineSlotCapacityLocked, this->GetPropertyTypes());
        if (instance->HasReadOnlyPropertiesInvisibleToTypeHandler())
        {
            newTypeHandler->ClearHasOnlyWritableDataProperties();
        }
        newTypeHandler->SetInstanceTypeHandler(instance);

        return newTypeHandler;
    }

    SimpleTypeHandler<1>* NullTypeHandlerBase::ConvertToSimpleType(DynamicObject* instance)
    {
        SimpleTypeHandler<1>* newTypeHandler = ConvertToTypeHandler<SimpleTypeHandler<1>>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertNullToSimpleCount++;
#endif
        return newTypeHandler;
    }


    SimpleDictionaryTypeHandler * NullTypeHandlerBase::ConvertToSimpleDictionaryType(DynamicObject * instance)
    {
        SimpleDictionaryTypeHandler* newTypeHandler = ConvertToTypeHandler<SimpleDictionaryTypeHandler>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertNullToSimpleDictionaryCount++;
#endif
        return newTypeHandler;
    }


    DictionaryTypeHandler * NullTypeHandlerBase::ConvertToDictionaryType(DynamicObject * instance)
    {
        DictionaryTypeHandler* newTypeHandler = ConvertToTypeHandler<DictionaryTypeHandler>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertNullToDictionaryCount++;
#endif
        return newTypeHandler;
    }


    ES5ArrayTypeHandler* NullTypeHandlerBase::ConvertToES5ArrayType(DynamicObject * instance)
    {
        ES5ArrayTypeHandler* newTypeHandler = ConvertToTypeHandler<ES5ArrayTypeHandler>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertNullToDictionaryCount++;
#endif
        return newTypeHandler;
    }


    BOOL NullTypeHandlerBase::SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        // Always check numeric propertyId. May create objectArray.
        uint32 indexVal;
        if (instance->GetScriptContext()->IsNumericPropertyId(propertyId, &indexVal))
        {
            return NullTypeHandlerBase::SetItemWithAttributes(instance, indexVal, value, attributes);
        }

        return this->AddProperty(instance, propertyId, value, attributes, info, flags, possibleSideEffects);
    }


    BOOL NullTypeHandlerBase::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {
        return false;
    }


    BOOL NullTypeHandlerBase::GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {
        return false;
    }


    DynamicTypeHandler* NullTypeHandlerBase::ConvertToTypeWithItemAttributes(DynamicObject* instance)
    {
        return JavascriptArray::Is(instance) ?
            ConvertToES5ArrayType(instance) : ConvertToDictionaryType(instance);
    }

    void NullTypeHandlerBase::SetIsPrototype(DynamicObject* instance)
    {
        if (!this->isPrototype)
        {
            // We don't force a type transition even when ChangeTypeOnProto() == true, because objects with NullTypeHandlers don't
            // have any properties, so there is nothing to invalidate.  Types with NullTypeHandlers also aren't cached in typeWithoutProperty
            // caches, so there will be no fast property add path that could skip prototype cache invalidation.
            NullTypeHandler<true>* protoTypeHandler = NullTypeHandler<true>::GetDefaultInstance();
            AssertMsg(protoTypeHandler->GetFlags() == (GetFlags() | IsPrototypeFlag), "Why did we change the flags of a NullTypeHandler?");
            Assert(this->GetIsInlineSlotCapacityLocked() == protoTypeHandler->GetIsInlineSlotCapacityLocked());
            protoTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection, GetPropertyTypes());
            SetInstanceTypeHandler(instance, protoTypeHandler);
        }
    }

    template<bool IsPrototypeTemplate>
    NullTypeHandler<IsPrototypeTemplate> NullTypeHandler<IsPrototypeTemplate>::defaultInstance;

    template class NullTypeHandler<false>;
    template class NullTypeHandler<true>;
}

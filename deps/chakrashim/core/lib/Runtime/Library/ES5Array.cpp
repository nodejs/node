//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    CompileAssert(sizeof(ES5Array) == sizeof(JavascriptArray));

    ES5ArrayType::ES5ArrayType(DynamicType* type)
        : DynamicType(type->GetScriptContext(), TypeIds_ES5Array, type->GetPrototype(), type->GetEntryPoint(), type->GetTypeHandler(), false, false)
    {
    }

    bool ES5Array::Is(Var instance)
    {
        return JavascriptOperators::GetTypeId(instance) == TypeIds_ES5Array;
    }

    ES5Array* ES5Array::FromVar(Var instance)
    {
        Assert(Is(instance));
        return static_cast<ES5Array*>(instance);
    }

    DynamicType* ES5Array::DuplicateType()
    {
        return RecyclerNew(GetScriptContext()->GetRecycler(), ES5ArrayType, this->GetDynamicType());
    }

    bool ES5Array::IsLengthWritable() const
    {
        return GetTypeHandler()->IsLengthWritable();
    }

    BOOL ES5Array::HasProperty(PropertyId propertyId)
    {
        if (propertyId == PropertyIds::length)
        {
            return true;
        }

        // Skip JavascriptArray override
        return DynamicObject::HasProperty(propertyId);
    }

    BOOL ES5Array::IsWritable(PropertyId propertyId)
    {
        if (propertyId == PropertyIds::length)
        {
            return IsLengthWritable();
        }

        return __super::IsWritable(propertyId);
    }

    BOOL ES5Array::SetEnumerable(PropertyId propertyId, BOOL value)
    {
        // Skip JavascriptArray override
        return DynamicObject::SetEnumerable(propertyId, value);
    }

    BOOL ES5Array::SetWritable(PropertyId propertyId, BOOL value)
    {
        // Skip JavascriptArray override
        return DynamicObject::SetWritable(propertyId, value);
    }

    BOOL ES5Array::SetConfigurable(PropertyId propertyId, BOOL value)
    {
        // Skip JavascriptArray override
        return DynamicObject::SetConfigurable(propertyId, value);
    }

    BOOL ES5Array::SetAttributes(PropertyId propertyId, PropertyAttributes attributes)
    {
        // Skip JavascriptArray override
        return DynamicObject::SetAttributes(propertyId, attributes);
    }

    BOOL ES5Array::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        BOOL result;
        if (GetPropertyBuiltIns(propertyId, value, &result))
        {
            return result;
        }

        // Skip JavascriptArray override
        return DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL ES5Array::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, &result))
        {
            return result;
        }

        // Skip JavascriptArray override
        return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    bool ES5Array::GetPropertyBuiltIns(PropertyId propertyId, Var* value, BOOL* result)
    {
        if (propertyId == PropertyIds::length)
        {
            *value = JavascriptNumber::ToVar(this->GetLength(), GetScriptContext());
            *result = true;
            return true;
        }

        return false;
    }

    BOOL ES5Array::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return ES5Array::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    // Convert a Var to array length, throw RangeError if value is not valid for array length.
    uint32 ES5Array::ToLengthValue(Var value, ScriptContext* scriptContext)
    {
        if (TaggedInt::Is(value))
        {
            int32 newLen = TaggedInt::ToInt32(value);
            if (newLen < 0)
            {
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
            }
            return static_cast<uint32>(newLen);
        }
        else
        {
            uint32 newLen = JavascriptConversion::ToUInt32(value, scriptContext);
            if (newLen != JavascriptConversion::ToNumber(value, scriptContext))
            {
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
            }
            return newLen;
        }
    }

    DescriptorFlags ES5Array::GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        DescriptorFlags result;
        if (GetSetterBuiltIns(propertyId, info, &result))
        {
            return result;
        }

        return DynamicObject::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags ES5Array::GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        DescriptorFlags result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetSetterBuiltIns(propertyRecord->GetPropertyId(), info, &result))
        {
            return result;
        }

        return DynamicObject::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    bool ES5Array::GetSetterBuiltIns(PropertyId propertyId, PropertyValueInfo* info, DescriptorFlags* result)
    {
        if (propertyId == PropertyIds::length)
        {
            PropertyValueInfo::SetNoCache(info, this);
            *result =  IsLengthWritable() ? WritableData : Data;
            return true;
        }

        return false;
    }

    BOOL ES5Array::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags propertyOperationFlags, PropertyValueInfo* info)
    {
        BOOL result;
        if (SetPropertyBuiltIns(propertyId, value, propertyOperationFlags, &result))
        {
            return result;
        }

        return __super::SetProperty(propertyId, value, propertyOperationFlags, info);
    }

    BOOL ES5Array::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags propertyOperationFlags, PropertyValueInfo* info)
    {
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && SetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, propertyOperationFlags, &result))
        {
            return result;
        }

        return __super::SetProperty(propertyNameString, value, propertyOperationFlags, info);
    }

    bool ES5Array::SetPropertyBuiltIns(PropertyId propertyId, Var value, PropertyOperationFlags propertyOperationFlags, BOOL* result)
    {
        ScriptContext* scriptContext = GetScriptContext();

        if (propertyId == PropertyIds::length)
        {
            if (!GetTypeHandler()->IsLengthWritable())
            {
                *result = false; // reject
                return true;
            }

            uint32 newLen = ToLengthValue(value, scriptContext);
            GetTypeHandler()->SetLength(this, newLen, propertyOperationFlags);
            *result = true;
            return true;
        }

        return false;
    }

    BOOL ES5Array::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        if (propertyId == PropertyIds::length)
        {
            Assert(attributes == PropertyWritable);
            Assert(IsWritable(propertyId) && !IsConfigurable(propertyId) && !IsEnumerable(propertyId));

            uint32 newLen = ToLengthValue(value, GetScriptContext());
            GetTypeHandler()->SetLength(this, newLen, PropertyOperation_None);
            return true;
        }

        return __super::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    BOOL ES5Array::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {
        // Skip JavascriptArray override
        return DynamicObject::DeleteItem(index, flags);
    }

    BOOL ES5Array::HasItem(uint32 index)
    {
        // Skip JavascriptArray override
        return DynamicObject::HasItem(index);
    }

    BOOL ES5Array::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {
        // Skip JavascriptArray override
        return DynamicObject::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL ES5Array::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {
        // Skip JavascriptArray override
        return DynamicObject::GetItemReference(originalInstance, index, value, requestContext);
    }

    DescriptorFlags ES5Array::GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {
        // Skip JavascriptArray override
        return DynamicObject::GetItemSetter(index, setterValue, requestContext);
    }

    BOOL ES5Array::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {
        // Skip JavascriptArray override
        return DynamicObject::SetItem(index, value, flags);
    }

    BOOL ES5Array::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {
        // Skip JavascriptArray override
        return DynamicObject::SetAccessors(propertyId, getter, setter, flags);
    }

    BOOL ES5Array::PreventExtensions()
    {
        // Skip JavascriptArray override
        return DynamicObject::PreventExtensions();
    }

    BOOL ES5Array::Seal()
    {
        // Skip JavascriptArray override
        return DynamicObject::Seal();
    }

    BOOL ES5Array::Freeze()
    {
        // Skip JavascriptArray override
        return DynamicObject::Freeze();
    }

    BOOL ES5Array::GetEnumerator(BOOL enumNonEnumerable, Var* enumerator, ScriptContext* requestContext, bool preferSnapshotSemantics, bool enumSymbols)
    {
        // Pass this as originalInstance
        return ES5Array::GetEnumerator(this, enumNonEnumerable, enumerator, requestContext, preferSnapshotSemantics, enumSymbols);
    }

    BOOL ES5Array::GetEnumerator(Var originalInstance, BOOL enumNonEnumerable, Var* enumerator, ScriptContext* requestContext, bool preferSnapshotSemantics, bool enumSymbols)
    {
        // ES5Array does not support compat mode, ignore preferSnapshotSemantics
        *enumerator = RecyclerNew(GetScriptContext()->GetRecycler(), ES5ArrayEnumerator, originalInstance, this, requestContext, enumNonEnumerable, enumSymbols);
        return true;
    }

    BOOL ES5Array::GetNonIndexEnumerator(Var* enumerator, ScriptContext* requestContext)
    {
        *enumerator = RecyclerNew(GetScriptContext()->GetRecycler(), ES5ArrayNonIndexEnumerator, this, this, requestContext, false);
        return true;
    }

    BOOL ES5Array::IsItemEnumerable(uint32 index)
    {
        return GetTypeHandler()->IsItemEnumerable(this, index);
    }

    BOOL ES5Array::SetItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes)
    {
        return GetTypeHandler()->SetItemWithAttributes(this, index, value, attributes);
    }

    BOOL ES5Array::SetItemAttributes(uint32 index, PropertyAttributes attributes)
    {
        return GetTypeHandler()->SetItemAttributes(this, index, attributes);
    }

    BOOL ES5Array::SetItemAccessors(uint32 index, Var getter, Var setter)
    {
        return GetTypeHandler()->SetItemAccessors(this, index, getter, setter);
    }

    BOOL ES5Array::IsObjectArrayFrozen()
    {
        return GetTypeHandler()->IsObjectArrayFrozen(this);
    }

    BOOL ES5Array::IsValidDescriptorToken(void * descriptorValidationToken) const
    {
        return GetTypeHandler()->IsValidDescriptorToken(descriptorValidationToken);
    }
    uint32 ES5Array::GetNextDescriptor(uint32 key, IndexPropertyDescriptor** descriptor, void ** descriptorValidationToken)
    {
        return GetTypeHandler()->GetNextDescriptor(key, descriptor, descriptorValidationToken);
    }

    BOOL ES5Array::GetDescriptor(uint32 index, Js::IndexPropertyDescriptor **ppDescriptor)
    {
        return GetTypeHandler()->GetDescriptor(index, ppDescriptor);
    }
}

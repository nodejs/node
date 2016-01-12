//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// Implementation for typed arrays based on ArrayBuffer.
// There is one nested ArrayBuffer for each typed array. Multiple typed array
// can share the same array buffer.
//----------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#define INSTANTIATE_BUILT_IN_ENTRYPOINTS(typeName) \
    template Var typeName::NewInstance(RecyclableObject* function, CallInfo callInfo, ...); \
    template Var typeName::EntrySet(RecyclableObject* function, CallInfo callInfo, ...); \
    template Var typeName::EntrySubarray(RecyclableObject* function, CallInfo callInfo, ...); \

namespace Js
{
    /*static*/
    PropertyId const TypedArrayBase::specialPropertyIds[] =
    {
        PropertyIds::length,
        PropertyIds::buffer,
        PropertyIds::byteOffset,
        PropertyIds::byteLength,
        PropertyIds::BYTES_PER_ELEMENT
    };

    // explicitly instantiate these function for the built in entry point FunctionInfo
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Int8Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Uint8Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Uint8ClampedArray)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Int16Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Uint16Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Int32Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Uint32Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Float32Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Float64Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Int64Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Uint64Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(BoolArray)

    TypedArrayBase::TypedArrayBase(ArrayBuffer* arrayBuffer, uint32 offSet, uint mappedLength, uint elementSize, DynamicType* type) :
        ArrayBufferParent(type, mappedLength, arrayBuffer),
        byteOffset(offSet),
        BYTES_PER_ELEMENT(elementSize)
    {
    }

    Var TypedArrayBase::CreateNewInstance(Arguments& args, ScriptContext* scriptContext, uint32 elementSize, PFNCreateTypedArray pfnCreateTypedArray)
    {
        uint32 byteLength = 0;
        int32 offSet = 0;
        int32 mappedLength = -1;
        int32 elementCount = 0;
        ArrayBuffer* arrayBuffer = nullptr;
        TypedArrayBase* typedArraySource = nullptr;
        RecyclableObject* jsArraySource = nullptr;
        bool fromExternalObject = false;

        // Handle first argument - try to get an ArrayBuffer
        if (args.Info.Count > 1)
        {
            Var firstArgument = args[1];
            if (TypedArrayBase::Is(firstArgument))
            {
                // Constructor(TypedArray array)
                typedArraySource = static_cast<TypedArrayBase*>(firstArgument);
                if (typedArraySource->IsDetachedBuffer())
                {
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray]");
                }

                elementCount = typedArraySource->GetLength();
                if ((uint32)elementCount >= ArrayBuffer::MaxArrayBufferLength/elementSize)
                {
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_InvalidTypedArrayLength);
                }
            }
            else if (ArrayBuffer::Is(firstArgument))
            {
                // Constructor(ArrayBuffer buffer,
                //  optional unsigned long byteOffset, optional unsigned long length)
                arrayBuffer = ArrayBuffer::FromVar(firstArgument);
                if (arrayBuffer->IsDetached())
                {
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray]");
                }
            }
            else if (JavascriptNumber::Is(firstArgument) ||
                    TaggedInt::Is(firstArgument))
            {
                elementCount = ToLengthChecked(firstArgument, elementSize, scriptContext);
            }
            else
            {
                if (JavascriptOperators::IsObject(firstArgument) && JavascriptConversion::ToObject(firstArgument, scriptContext, &jsArraySource))
                {
                    HRESULT hr = scriptContext->GetHostScriptContext()->ArrayBufferFromExternalObject(jsArraySource, &arrayBuffer);
                    switch (hr)
                    {
                    case S_OK:
                        // We found an IBuffer
                        fromExternalObject = true;
                        OUTPUT_TRACE(TypedArrayPhase, L"Projection ArrayBuffer query succeeded with HR=0x%08X\n", hr);
                        // We have an ArrayBuffer now, so we can skip all the object probing.
                        break;

                    case S_FALSE:
                        // We didn't find an IBuffer - fall through
                        OUTPUT_TRACE(TypedArrayPhase, L"Projection ArrayBuffer query aborted safely with HR=0x%08X (non-handled type)\n", hr);
                        break;

                    default:
                        // Any FAILURE HRESULT or unexpected HRESULT
                        OUTPUT_TRACE(TypedArrayPhase, L"Projection ArrayBuffer query failed with HR=0x%08X\n", hr);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidTypedArray_Constructor);
                        break;
                    }
                    if (!fromExternalObject)
                    {
                        Var lengthVar = JavascriptOperators::OP_GetProperty(jsArraySource, PropertyIds::length, scriptContext);
                        if (JavascriptOperators::GetTypeId(lengthVar) == TypeIds_Undefined)
                        {
                            JavascriptError::ThrowTypeError(
                                scriptContext, JSERR_InvalidTypedArray_Constructor);
                        }
                        elementCount = ToLengthChecked(lengthVar, elementSize, scriptContext);
                    }
                }
                else
                {
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidTypedArray_Constructor);
                }
            }
        }

        // If we got an ArrayBuffer, we can continue to process arguments.
        if (arrayBuffer != nullptr)
        {
            byteLength = arrayBuffer->GetByteLength();
            if (args.Info.Count > 2)
            {
                Var secondArgument = args[2];
                offSet = JavascriptConversion::ToInt32(secondArgument, scriptContext);

                // we can start the mapping from the end of the incoming buffer, but with a map of 0 length.
                // User can't really do anything useful with the typed array but apparently this is allowed.
                if (offSet < 0 ||
                    (uint32)offSet > byteLength ||
                    (offSet % elementSize) != 0)
                {
                    JavascriptError::ThrowRangeError(
                        scriptContext, JSERR_InvalidTypedArrayLength);
                }
            }

            if (args.Info.Count > 3 && !JavascriptOperators::IsUndefinedObject(args[3]))
            {
                Var thirdArgument = args[3];
                mappedLength = JavascriptConversion::ToInt32(thirdArgument, scriptContext);

                if (mappedLength < 0 ||
                    (uint32)mappedLength > ArrayBuffer::MaxArrayBufferLength / elementSize ||
                    (uint32)mappedLength > (byteLength - offSet)/ elementSize)
                {
                    JavascriptError::ThrowRangeError(
                        scriptContext, JSERR_InvalidTypedArrayLength);
                }
            }
            else
            {
                if ((byteLength - offSet) % elementSize != 0)
                {
                    JavascriptError::ThrowRangeError(
                        scriptContext, JSERR_InvalidTypedArrayLength);
                }
                mappedLength = (byteLength - offSet)/elementSize;
            }
        }
        else {
            // Null arrayBuffer - could be new constructor or copy constructor.
            byteLength = elementCount * elementSize;
            arrayBuffer = scriptContext->GetLibrary()->CreateArrayBuffer(byteLength);
        }

        if (arrayBuffer->IsDetached())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray]");
        }

        if (mappedLength == -1)
        {
            mappedLength = (byteLength - offSet)/elementSize;
        }

        // Create and set the array based on the source.
        TypedArrayBase* newArray  = static_cast<TypedArrayBase*>(pfnCreateTypedArray(arrayBuffer, offSet, mappedLength, scriptContext->GetLibrary()));
        if (fromExternalObject)
        {
            // No need to copy externally provided buffer
            OUTPUT_TRACE(TypedArrayPhase, L"Created a TypedArray from an external buffer source\n");
        }
        else if (typedArraySource)
        {
            newArray->Set(typedArraySource, offSet);
            OUTPUT_TRACE(TypedArrayPhase, L"Created a TypedArray from a typed array source\n");
        }
        else if (jsArraySource)
        {
            newArray->SetObject(jsArraySource, newArray->GetLength(), offSet);
            OUTPUT_TRACE(TypedArrayPhase, L"Created a TypedArray from a JavaScript array source\n");
        }

        return newArray;
    }

    int32 TypedArrayBase::ToLengthChecked(Var lengthVar, uint32 elementSize, ScriptContext* scriptContext)
    {
        int32 length = JavascriptConversion::ToInt32(lengthVar, scriptContext);
        if (length < 0 ||
            (uint32)length >= ArrayBuffer::MaxArrayBufferLength / elementSize ||
            !JavascriptConversion::SameValueZero(lengthVar, JavascriptNumber::ToVar(length, scriptContext)))
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_InvalidTypedArrayLength);
        }

        return length;
    }

    Var TypedArrayBase::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        function->GetScriptContext()->GetThreadContext()->ProbeStack(Js::Constants::MinStackDefault, function->GetScriptContext());

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && RecyclableObject::Is(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);

        JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidTypedArray_Constructor);
    }

    template <typename TypeName, bool clamped, bool virtualAllocated>
    Var TypedArray<TypeName, clamped, virtualAllocated>::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        function->GetScriptContext()->GetThreadContext()->ProbeStack(Js::Constants::MinStackDefault, function->GetScriptContext());

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && RecyclableObject::Is(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);

        if (!(callInfo.Flags & CallFlags_New) || (newTarget && JavascriptOperators::IsUndefinedObject(newTarget)))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, L"[TypedArray]");
        }

        Var object = TypedArrayBase::CreateNewInstance(args, scriptContext, sizeof(TypeName), TypedArray<TypeName, clamped, virtualAllocated>::Create);

#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {
            object = Js::JavascriptProxy::AutoProxyWrapper(object);
        }
#endif
        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), RecyclableObject::FromVar(object), nullptr, scriptContext) :
            object;
    };

    inline BOOL TypedArrayBase::IsBuiltinProperty(PropertyId propertyId)
    {
        // We only return the builtins in pre-ES6 mode
        if (!GetScriptContext()->GetConfig()->IsES6TypedArrayExtensionsEnabled() &&
            (propertyId == PropertyIds::length ||
            propertyId == PropertyIds::buffer ||
            propertyId == PropertyIds::byteOffset ||
            propertyId == PropertyIds::byteLength ||
            propertyId == PropertyIds::BYTES_PER_ELEMENT))
        {
            return TRUE;
        }

        return FALSE;
    }

    BOOL TypedArrayBase::HasProperty(PropertyId propertyId)
    {
        if (IsBuiltinProperty(propertyId))
        {
            return true;
        }

        uint32 index = 0;
        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index) && (index < this->GetLength()))
        {
            // All the slots within the length of the array are valid.
            return true;
        }
        return DynamicObject::HasProperty(propertyId);
    }

    BOOL TypedArrayBase::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {
        if (IsBuiltinProperty(propertyId))
        {
            return false;
        }
        uint32 index = 0;
        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index) && (index < this->GetLength()))
        {
            // All the slots within the length of the array are valid.
            return false;
        }
        return DynamicObject::DeleteProperty(propertyId, flags);

    }

    BOOL TypedArrayBase::GetSpecialPropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext)
    {
        uint length = GetSpecialPropertyCount();
        if (index < length)
        {
            *propertyName = requestContext->GetPropertyString(specialPropertyIds[index]);
            return true;
        }
        return false;
    }

    // Returns the number of special non-enumerable properties this type has.
    uint TypedArrayBase::GetSpecialPropertyCount() const
    {
        if (GetScriptContext()->GetConfig()->IsES6TypedArrayExtensionsEnabled())
        {
            return 0;
        }

        return _countof(specialPropertyIds);
    }

    // Returns the list of special non-enumerable properties for the type.
    PropertyId const * TypedArrayBase::GetSpecialPropertyIds() const
    {
        return specialPropertyIds;
    }

    BOOL TypedArrayBase::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return TypedArrayBase::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL TypedArrayBase::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        uint32 index = 0;
        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index))
        {
            *value = this->DirectGetItem(index);
            if (JavascriptOperators::GetTypeId(*value) == Js::TypeIds_Undefined)
            {
                return false;
            }
            return true;
        }

        if (GetPropertyBuiltIns(propertyId, value))
        {
            return true;
        }

        return DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL TypedArrayBase::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value))
        {
            return true;
        }

        return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    BOOL TypedArrayBase::GetPropertyBuiltIns(PropertyId propertyId, Var* value)
    {
        //
        // Only return built-ins for pre-ES6 mode
        //
        if (!GetScriptContext()->GetConfig()->IsES6TypedArrayExtensionsEnabled())
        {
            switch (propertyId)
            {
            case PropertyIds::length:
                *value = JavascriptNumber::ToVar(this->GetLength(), GetScriptContext());
                return true;
            case PropertyIds::byteLength:
                *value = JavascriptNumber::ToVar(this->GetByteLength(), GetScriptContext());
                return true;
            case PropertyIds::byteOffset:
                *value = JavascriptNumber::ToVar(this->GetByteOffset(), GetScriptContext());
                return true;
            case PropertyIds::buffer:
                *value = GetArrayBuffer();
                return true;
            case PropertyIds::BYTES_PER_ELEMENT:
                *value = JavascriptNumber::ToVar(this->GetBytesPerElement(), GetScriptContext());
                return true;
            }
        }

        return false;
    }

    BOOL TypedArrayBase::HasItem(uint32 index)
    {
        if (this->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_DetachedTypedArray);
        }

        if (index < GetLength())
        {
            return true;
        }
        return false;
    }

    BOOL TypedArrayBase::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {
        *value = DirectGetItem(index);
        return true;
    }

    BOOL TypedArrayBase::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {
        *value = DirectGetItem(index);
        return true;
    }

    BOOL TypedArrayBase::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        uint32 index;

        if (IsBuiltinProperty(propertyId))
        {
            return false;
        }
        else if (GetScriptContext()->IsNumericPropertyId(propertyId, &index))
        {
            this->DirectSetItem(index, value, index >= GetLength());
            return true;
        }
        else
        {
            return DynamicObject::SetProperty(propertyId, value, flags, info);
        }
    }

    BOOL TypedArrayBase::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && IsBuiltinProperty(propertyRecord->GetPropertyId()))
        {
            return false;
        }

        return DynamicObject::SetProperty(propertyNameString, value, flags, info);
    }

    BOOL TypedArrayBase::GetEnumerator(BOOL enumNonEnumerable, Var* enumerator, ScriptContext* scriptContext, bool preferSnapshotSemantics, bool enumSymbols)
    {
        *enumerator = RecyclerNew(scriptContext->GetRecycler(), TypedArrayEnumerator, enumNonEnumerable, this, scriptContext, enumSymbols);
        return true;
    }

    BOOL TypedArrayBase::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {
        // Skip set item if index >= GetLength()
        DirectSetItem(index, value, index >= GetLength());
        return true;
    }

    BOOL TypedArrayBase::IsEnumerable(PropertyId propertyId)
    {
        if (IsBuiltinProperty(propertyId))
        {
            return false;
        }
        uint32 index = 0;
        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index))
        {
            return true;
        }
        return DynamicObject::IsEnumerable(propertyId);
    }

    BOOL TypedArrayBase::IsConfigurable(PropertyId propertyId)
    {
        if (IsBuiltinProperty(propertyId))
        {
            return false;
        }
        uint32 index = 0;
        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index))
        {
            return false;
        }
        return DynamicObject::IsConfigurable(propertyId);
    }


    BOOL TypedArrayBase::InitProperty(Js::PropertyId propertyId, Js::Var value, PropertyOperationFlags flags, Js::PropertyValueInfo* info)
    {
        return SetProperty(propertyId, value, flags, info);
    }

    BOOL TypedArrayBase::IsWritable(PropertyId propertyId)
    {
        if (IsBuiltinProperty(propertyId))
        {
            return false;
        }
        uint32 index = 0;
        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index))
        {
            return true;
        }
        return DynamicObject::IsWritable(propertyId);
    }


    BOOL TypedArrayBase::SetEnumerable(PropertyId propertyId, BOOL value)
    {
        ScriptContext* scriptContext = GetScriptContext();
        if (IsBuiltinProperty(propertyId))
        {
            if (value)
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                    scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }

        uint32 index;
        // all numeric properties are enumerable
        if (scriptContext->IsNumericPropertyId(propertyId, &index) )
        {
            if (!value)
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                    scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }

        return __super::SetEnumerable(propertyId, value);
    }

    BOOL TypedArrayBase::SetWritable(PropertyId propertyId, BOOL value)
    {
        ScriptContext* scriptContext = GetScriptContext();
        if (IsBuiltinProperty(propertyId))
        {
            if (value)
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                    scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }

        uint32 index;
        // default is writable
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            if (!value)
            {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }

        return __super::SetWritable(propertyId, value);
    }

    BOOL TypedArrayBase::SetConfigurable(PropertyId propertyId, BOOL value)
    {
        ScriptContext* scriptContext = GetScriptContext();
        if (IsBuiltinProperty(propertyId))
        {
            if (value)
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                    scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }

        uint32 index;
        // default is not configurable
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            if (value)
            {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }

        return __super::SetConfigurable(propertyId, value);
    }

    BOOL TypedArrayBase::SetAttributes(PropertyId propertyId, PropertyAttributes attributes)
    {
        ScriptContext* scriptContext = this->GetScriptContext();

        if (IsBuiltinProperty(propertyId))
        {
            if (attributes != PropertyNone)
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                    scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            VerifySetItemAttributes(propertyId, attributes);
            return true;
        }

        return __super::SetAttributes(propertyId, attributes);
    }

    BOOL TypedArrayBase::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        if (IsBuiltinProperty(propertyId))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                GetScriptContext()->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
        }

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                GetScriptContext()->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
        }

        return __super::SetAccessors(propertyId, getter, setter, flags);
    }

    BOOL TypedArrayBase::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {
        ScriptContext* scriptContext = GetScriptContext();

        if (IsBuiltinProperty(propertyId))
        {
            if (attributes != PropertyNone)
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                    scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            VerifySetItemAttributes(propertyId, attributes);
            return SetItem(index, value);
        }

        return __super::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    BOOL TypedArrayBase::SetItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes)
    {
        VerifySetItemAttributes(Constants::NoProperty, attributes);
        return SetItem(index, value);
    }

    BOOL TypedArrayBase::Is(Var aValue)
    {
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return Is(typeId);
    }

    BOOL TypedArrayBase::Is(TypeId typeId)
    {
        return typeId >= TypeIds_TypedArrayMin && typeId <= TypeIds_TypedArrayMax;
    }

    TypedArrayBase* TypedArrayBase::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "must be a typed array");
        return static_cast<TypedArrayBase*>(aValue);
    }

    BOOL TypedArrayBase::IsDetachedTypedArray(Var aValue)
    {
        return Is(aValue) && FromVar(aValue)->IsDetachedBuffer();
    }

    void TypedArrayBase::Set(TypedArrayBase* source, uint32 offset)
    {
        uint32 sourceLength = source->GetLength();
        uint32 totalLength;

        if (UInt32Math::Add(offset, sourceLength, &totalLength) ||
            (totalLength > GetLength()))
        {
            JavascriptError::ThrowTypeError(
                GetScriptContext(), JSERR_InvalidTypedArrayLength);
        }

        if (source->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_DetachedTypedArray);
        }

        // memmove buffer if views have same bit representation.
        // types of the same size are compatible, with the following exceptions:
        // - we cannot memmove between float and int arrays, due to different bit pattern
        // - we cannot memmove to a uint8 clamped array from an int8 array, due to negatives rounding to 0
        if (GetTypeId() == source->GetTypeId()
            || (GetBytesPerElement() == source->GetBytesPerElement()
            && !(Uint8ClampedArray::Is(this) && Int8Array::Is(source))
            && !Float32Array::Is(this) && !Float32Array::Is(source)
            && !Float64Array::Is(this) && !Float64Array::Is(source)))
        {
            const size_t offsetInBytes = offset * BYTES_PER_ELEMENT;
            memmove_s(buffer + offsetInBytes,
                      GetByteLength() - offsetInBytes,
                      source->GetByteBuffer(),
                      source->GetByteLength());
        }
        else
        {
            if (source->GetArrayBuffer() != GetArrayBuffer())
            {
                for (uint32 i = 0; i < sourceLength; i++)
                {
                    DirectSetItem(offset + i, source->DirectGetItem(i), false);
                }
            }
            else
            {
                // We can have the source and destination coming from the same buffer. element size, start offset, and
                // length for source and dest typed array can be different. Use a separate tmp buffer to copy the elements.
                Js::JavascriptArray* tmpArray = GetScriptContext()->GetLibrary()->CreateArray(sourceLength);
                for (uint32 i = 0; i < sourceLength; i++)
                {
                    tmpArray->SetItem(i, source->DirectGetItem(i), PropertyOperation_None);
                }
                for (uint32 i = 0; i < sourceLength; i++)
                {
                    DirectSetItem(offset + i, tmpArray->DirectGetItem(i), false);
                }
            }
        }
    }

    // Getting length from the source object can detach the typedarray, and thus set it's length as 0,
    // this is observable because RangeError will be thrown instead of a TypeError further down the line
    void TypedArrayBase::SetObject(RecyclableObject* source, uint32 targetLength, uint32 offset)
    {
        ScriptContext* scriptContext = GetScriptContext();
        uint32 sourceLength = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetProperty(source, PropertyIds::length, scriptContext), scriptContext);
        uint32 totalLength;
        if (UInt32Math::Add(offset, sourceLength, &totalLength) ||
            (totalLength > targetLength))
        {
            JavascriptError::ThrowRangeError(
                GetScriptContext(), JSERR_InvalidTypedArrayLength);
        }
        Var itemValue;
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        for (uint32 i = 0; i < sourceLength; i ++)
        {
            if (!source->GetItem(source, i, &itemValue, scriptContext))
            {
                itemValue = undefinedValue;
            }
            DirectSetItem(offset + i, itemValue, false);
        }
    }

    HRESULT TypedArrayBase::GetBuffer(Var instance, ArrayBuffer** outBuffer, uint32* outOffset, uint32* outLength)
    {
        HRESULT hr = NOERROR;
        if (Js::TypedArrayBase::Is(instance))
        {
            Js::TypedArrayBase* typedArrayBase = Js::TypedArrayBase::FromVar(instance);
            *outBuffer = typedArrayBase->GetArrayBuffer();
            *outOffset = typedArrayBase->GetByteOffset();
            *outLength = typedArrayBase->GetByteLength();
        }
        else if (Js::ArrayBuffer::Is(instance))
        {
            Js::ArrayBuffer* buffer = Js::ArrayBuffer::FromVar(instance);
            *outBuffer = buffer;
            *outOffset = 0;
            *outLength = buffer->GetByteLength();
        }
        else if (Js::DataView::Is(instance))
        {
            Js::DataView* dView = Js::DataView::FromVar(instance);
            *outBuffer = dView->GetArrayBuffer();
            *outOffset = dView->GetByteOffset();
            *outLength = dView->GetLength();
        }
        else
        {
            hr = E_INVALIDARG;
        }
        return hr;

    }

    template <typename TypeName, bool clamped, bool virtualAllocated>
    TypedArray<TypeName, clamped, virtualAllocated>::TypedArray(ArrayBuffer* arrayBuffer, uint32 byteOffset, uint32 mappedLength, DynamicType* type) :
        TypedArrayBase(arrayBuffer, byteOffset, mappedLength, sizeof(TypeName), type)
    {
        AssertMsg(arrayBuffer->GetByteLength() >= byteOffset, "invalid offset");
        AssertMsg(mappedLength*sizeof(TypeName)+byteOffset <= arrayBuffer->GetByteLength(), "invalid length");
        buffer = arrayBuffer->GetBuffer() + byteOffset;
        if (arrayBuffer->IsValidVirtualBufferLength(arrayBuffer->GetByteLength()) &&
             (byteOffset == 0) &&
             (mappedLength == (arrayBuffer->GetByteLength() / sizeof(TypeName)))
           )
        {
            // update the vtable
            switch (type->GetTypeId())
            {
            case TypeIds_Int8Array:
                VirtualTableInfo<Int8VirtualArray>::SetVirtualTable(this);
                break;
            case TypeIds_Uint8Array:
                VirtualTableInfo<Uint8VirtualArray>::SetVirtualTable(this);
                break;
            case TypeIds_Uint8ClampedArray:
                VirtualTableInfo<Uint8ClampedVirtualArray>::SetVirtualTable(this);
                break;
            case TypeIds_Int16Array:
                VirtualTableInfo<Int16VirtualArray>::SetVirtualTable(this);
                break;
            case TypeIds_Uint16Array:
                VirtualTableInfo<Uint16VirtualArray>::SetVirtualTable(this);
                break;
            case TypeIds_Int32Array:
                VirtualTableInfo<Int32VirtualArray>::SetVirtualTable(this);
                break;
            case TypeIds_Uint32Array:
                VirtualTableInfo<Uint32VirtualArray>::SetVirtualTable(this);
                break;
            case TypeIds_Float32Array:
                VirtualTableInfo<Float32VirtualArray>::SetVirtualTable(this);
                break;
            case TypeIds_Float64Array:
                VirtualTableInfo<Float64VirtualArray>::SetVirtualTable(this);
                break;
            default:
                break;
            }
        }
    }

    template <typename TypeName, bool clamped, bool virtualAllocated>
    __inline Var TypedArray<TypeName, clamped, virtualAllocated>::Create(ArrayBuffer* arrayBuffer, uint32 byteOffSet, uint32 mappedLength, JavascriptLibrary* javascriptLibrary)
    {
        uint32 totalLength, mappedByteLength;

        if (UInt32Math::Mul(mappedLength, sizeof(TypeName), &mappedByteLength) ||
            UInt32Math::Add(byteOffSet, mappedByteLength, &totalLength) ||
            (totalLength > arrayBuffer->GetByteLength()))
        {
            JavascriptError::ThrowRangeError(arrayBuffer->GetScriptContext(), JSERR_InvalidTypedArrayLength);
        }

        DynamicType *type = javascriptLibrary->GetTypedArrayType<TypeName, clamped>(0);
        return RecyclerNew(javascriptLibrary->GetRecycler(), TypedArray, arrayBuffer, byteOffSet, mappedLength, type);

    }

    Var TypedArrayBase::EntryGetterBuffer(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        TypedArrayBase* typedArray = TypedArrayBase::FromVar(args[0]);
        ArrayBuffer* arrayBuffer = typedArray->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        return arrayBuffer;
    }

    Var TypedArrayBase::EntryGetterByteLength(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        TypedArrayBase* typedArray = TypedArrayBase::FromVar(args[0]);
        ArrayBuffer* arrayBuffer = typedArray->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }
        else if (arrayBuffer->IsDetached())
        {
            return TaggedInt::ToVarUnchecked(0);
        }

        return JavascriptNumber::ToVar(typedArray->GetByteLength(), scriptContext);
    }

    Var TypedArrayBase::EntryGetterByteOffset(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        TypedArrayBase* typedArray = TypedArrayBase::FromVar(args[0]);
        ArrayBuffer* arrayBuffer = typedArray->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }
        else if (arrayBuffer->IsDetached())
        {
            return TaggedInt::ToVarUnchecked(0);
        }

        return JavascriptNumber::ToVar(typedArray->GetByteOffset(), scriptContext);
    }

    Var TypedArrayBase::EntryGetterLength(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        TypedArrayBase* typedArray = TypedArrayBase::FromVar(args[0]);
        ArrayBuffer* arrayBuffer = typedArray->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }
        else if (arrayBuffer->IsDetached())
        {
            return TaggedInt::ToVarUnchecked(0);
        }

        return JavascriptNumber::ToVar(typedArray->GetLength(), scriptContext);
    }

    Var TypedArrayBase::EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);

        Assert(args.Info.Count > 0);

        return args[0];
    }

    Var TypedArrayBase::EntryGetterSymbolToStringTag(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        Var name;

        switch (JavascriptOperators::GetTypeId(args[0]))
        {
        case TypeIds_Int8Array:
            name = library->CreateStringFromCppLiteral(L"Int8Array");
            break;

        case TypeIds_Uint8Array:
            name = library->CreateStringFromCppLiteral(L"Uint8Array");
            break;

        case TypeIds_Uint8ClampedArray:
            name = library->CreateStringFromCppLiteral(L"Uint8ClampedArray");
            break;

        case TypeIds_Int16Array:
            name = library->CreateStringFromCppLiteral(L"Int16Array");
            break;

        case TypeIds_Uint16Array:
            name = library->CreateStringFromCppLiteral(L"Uint16Array");
            break;

        case TypeIds_Int32Array:
            name = library->CreateStringFromCppLiteral(L"Int32Array");
            break;

        case TypeIds_Uint32Array:
            name = library->CreateStringFromCppLiteral(L"Uint32Array");
            break;

        case TypeIds_Float32Array:
            name = library->CreateStringFromCppLiteral(L"Float32Array");
            break;

        case TypeIds_Float64Array:
            name = library->CreateStringFromCppLiteral(L"Float64Array");
            break;

        default:
            name = library->GetUndefinedDisplayString();
            break;
        }

        return name;
    }

    Var TypedArrayBase::EntrySet(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        return CommonSet(args);
    }

    template <typename TypeName, bool clamped, bool virtualAllocated>
    Var TypedArray<TypeName, clamped, virtualAllocated>::EntrySet(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        Assert(!scriptContext->GetConfig()->IsES6TypedArrayExtensionsEnabled());

        // This method is only called in pre-ES6 compat modes. In those modes, we need to throw an error
        // if the this argument is not the same type as our TypedArray template instance.
        if (args.Info.Count == 0 || !TypedArray::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        return CommonSet(args);
    }

    Var TypedArrayBase::CommonSet(Arguments& args)
    {
        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        ScriptContext* scriptContext = typedArrayBase->GetScriptContext();
        int32 offset = 0;
        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_TypedArray_NeedSource);
        }
        if (args.Info.Count > 2)
        {
            offset = JavascriptConversion::ToInt32(args[2], scriptContext);
        }
        if (offset < 0) // If targetOffset < 0, then throw a RangeError exception.
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidTypedArrayLength);
        }
        if (TypedArrayBase::Is(args[1]))
        {
            TypedArrayBase* typedArraySource = TypedArrayBase::FromVar(args[1]);
            if (typedArraySource->IsDetachedBuffer() || typedArrayBase->IsDetachedBuffer()) // If IsDetachedBuffer(targetBuffer) is true, then throw a TypeError exception.
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.set");
            }
            typedArrayBase->Set(typedArraySource, (uint32)offset);
        }
        else
        {
            RecyclableObject* sourceArray;
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[1]);
#endif
            if (!JavascriptConversion::ToObject(args[1], scriptContext, &sourceArray))
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_TypedArray_NeedSource);
            }
            else if (typedArrayBase->IsDetachedBuffer())
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.set");
            }
            typedArrayBase->SetObject(sourceArray, typedArrayBase->GetLength(), (uint32)offset);
        }
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var TypedArrayBase::EntrySubarray(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TASubArrayCount);

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        return CommonSubarray(args);
    }

    template <typename TypeName, bool clamped, bool virtualAllocated>
    Var TypedArray<TypeName, clamped, virtualAllocated>::EntrySubarray(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        Assert(!scriptContext->GetConfig()->IsES6TypedArrayExtensionsEnabled());

        // This method is only called in pre-ES6 compat modes. In those modes, we need to throw an error
        // if the this argument is not the same type as our TypedArray template instance.
        if (args.Info.Count == 0 || !TypedArray::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        // In ES5, it is an error to call subarray with no begin parameter.
        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_InvalidTypedArraySubarrayLength);
        }

        return CommonSubarray(args);
    }

    Var TypedArrayBase::CommonSubarray(Arguments& args)
    {
        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        uint32 length = typedArrayBase->GetLength();
        ScriptContext* scriptContext = typedArrayBase->GetScriptContext();
        int32 begin = 0;

        if (args.Info.Count > 1)
        {
            begin = JavascriptConversion::ToInt32(args[1], scriptContext);
        }

        // The rule is:
        // The range specified by the begin and end values is clamped to the valid index range for the current array.
        // If the computed length of the new TypedArray would be negative, it is clamped to zero.
        if (begin < 0)
        {
            CompileAssert(ArrayBuffer::MaxArrayBufferLength <= INT_MAX);
            begin += length;
        }
        if (begin > (int32)length)
        {
            begin = length;
        }
        if (begin < 0)
        {
            begin = 0;
        }

        int32 end;
        if (args.Info.Count > 2 && !JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
        {
            end = JavascriptConversion::ToInt32(args[2], scriptContext);
            if (end < 0 )
            {
                CompileAssert(ArrayBuffer::MaxArrayBufferLength <= INT_MAX);
                end += length;
            }
            if (end > (int32)length)
            {
                // we need clamp the length to buffer length instead of last index.
                end = length;
            }
            if (end < 0)
            {
                end = 0;
            }
        }
        else
        {
            end = length;
        }
        if (end < begin)
        {
            end = begin;
        }

        return typedArrayBase->Subarray(begin, end);
    }

    template <typename TypeName, bool clamped, bool virtualAllocated>
    Var TypedArray<TypeName, clamped, virtualAllocated>::Subarray(uint32 begin, uint32 end)
    {
        Assert(end >= begin);

        Var newTypedArray;
        ScriptContext* scriptContext = this->GetScriptContext();
        ArrayBuffer* buffer = this->GetArrayBuffer();
        uint32 srcByteOffset = this->GetByteOffset();
        uint32 beginByteOffset = srcByteOffset + begin * BYTES_PER_ELEMENT;
        uint32 newLength = end - begin;

        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {
            JavascriptFunction* constructor =
                JavascriptFunction::FromVar(JavascriptOperators::SpeciesConstructor(this, TypedArrayBase::GetDefaultConstructor(this, scriptContext), scriptContext));

            Js::Var constructorArgs[] = { constructor, buffer, JavascriptNumber::ToVar(beginByteOffset, scriptContext), JavascriptNumber::ToVar(newLength, scriptContext) };
            Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
            newTypedArray = JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext);
        }
        else
        {
            newTypedArray = TypedArray<TypeName, clamped, virtualAllocated>::Create(buffer, beginByteOffset, newLength, scriptContext->GetLibrary());
        }

        return newTypedArray;
    }

    // %TypedArray%.from as described in ES6.0 (draft 22) Section 22.2.2.1
    Var TypedArrayBase::EntryFrom(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"[TypedArray].from");

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAFromCount);

        if (args.Info.Count < 1 || !JavascriptOperators::IsConstructor(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedFunction, L"[TypedArray].from");
        }

        RecyclableObject* constructor = RecyclableObject::FromVar(args[0]);
        RecyclableObject* items = nullptr;

        if (args.Info.Count < 2 || !JavascriptConversion::ToObject(args[1], scriptContext, &items))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"[TypedArray].from");
        }

        bool mapping = false;
        JavascriptFunction* mapFn = nullptr;
        Var mapFnThisArg = nullptr;

        if (args.Info.Count >= 3)
        {
            if (!JavascriptFunction::Is(args[2]))
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, L"[TypedArray].from");
            }

            mapFn = JavascriptFunction::FromVar(args[2]);

            if (args.Info.Count >= 4)
            {
                mapFnThisArg = args[3];
            }
            else
            {
                mapFnThisArg = library->GetUndefined();
            }

            mapping = true;
        }

        Var newObj;

        if (JavascriptOperators::IsIterable(items, scriptContext))
        {
            RecyclableObject* iterator = JavascriptOperators::GetIterator(items, scriptContext);
            Var nextValue;

            DECLARE_TEMP_GUEST_ALLOCATOR(tempAlloc);

            ACQUIRE_TEMP_GUEST_ALLOCATOR(tempAlloc, scriptContext, L"Runtime");
            {
                // Create a temporary list to hold the items returned from the iterator.
                // We will then iterate over this list and append those items into the object we will return.
                // We have to collect the items into this temporary list because we need to call the
                // new object constructor with a length of items and we don't know what length will be
                // until we iterate across all the items.
                // Consider: Could be possible to fast-path this in order to avoid creating the temporary list
                //       for types we know such as TypedArray. We know the length of a TypedArray but we still
                //       have to be careful in case there is a proxy which could return anything from [[Get]]
                //       or the built-in @@iterator has been replaced.
                JsUtil::List<Var, ArenaAllocator>* tempList = JsUtil::List<Var, ArenaAllocator>::New(tempAlloc);

                while (JavascriptOperators::IteratorStepAndValue(iterator, scriptContext, &nextValue))
                {
                    tempList->Add(nextValue);
                }

                uint32 len = tempList->Count();

                Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(len, scriptContext) };
                Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
                newObj = JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext);

                TypedArrayBase* newTypedArrayBase = nullptr;
                JavascriptArray* newArr = nullptr;

                if (TypedArrayBase::Is(newObj))
                {
                    newTypedArrayBase = TypedArrayBase::FromVar(newObj);
                }
                else if (JavascriptArray::Is(newObj))
                {
                    newArr = JavascriptArray::FromVar(newObj);
                }

                for (uint32 k = 0; k < len; k++)
                {
                    Var kValue = tempList->Item(k);

                    if (mapping)
                    {
                        Assert(mapFn != nullptr);
                        Assert(mapFnThisArg != nullptr);

                        Js::Var mapFnArgs[] = { mapFnThisArg, kValue, JavascriptNumber::ToVar(k, scriptContext) };
                        Js::CallInfo mapFnCallInfo(Js::CallFlags_Value, _countof(mapFnArgs));
                        kValue = mapFn->CallFunction(Js::Arguments(mapFnCallInfo, mapFnArgs));
                    }

                    // We're likely to have constructed a new TypedArray, but the constructor could return any object
                    if (newTypedArrayBase)
                    {
                        newTypedArrayBase->DirectSetItem(k, kValue, false);
                    }
                    else if (newArr)
                    {
                        newArr->SetItem(k, kValue, Js::PropertyOperation_ThrowIfNotExtensible);
                    }
                    else
                    {
                        JavascriptOperators::OP_SetElementI_UInt32(newObj, k, kValue, scriptContext, Js::PropertyOperation_ThrowIfNotExtensible);
                    }
                }
            }
            RELEASE_TEMP_GUEST_ALLOCATOR(tempAlloc, scriptContext);
        }
        else
        {
            Var lenValue = JavascriptOperators::OP_GetLength(items, scriptContext);
            uint32 len = JavascriptConversion::ToUInt32(lenValue, scriptContext);

            TypedArrayBase* itemsTypedArrayBase = nullptr;
            JavascriptArray* itemsArray = nullptr;

            if (TypedArrayBase::Is(items))
            {
                itemsTypedArrayBase = TypedArrayBase::FromVar(items);
            }
            else if (JavascriptArray::Is(items))
            {
                itemsArray = JavascriptArray::FromVar(items);
            }

            Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(len, scriptContext) };
            Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
            newObj = JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext);

            TypedArrayBase* newTypedArrayBase = nullptr;
            JavascriptArray* newArr = nullptr;

            if (TypedArrayBase::Is(newObj))
            {
                newTypedArrayBase = TypedArrayBase::FromVar(newObj);
            }
            else if (JavascriptArray::Is(newObj))
            {
                newArr = JavascriptArray::FromVar(newObj);
            }

            for (uint32 k = 0; k < len; k++)
            {
                Var kValue;

                // The items source could be anything, but we already know if it's a TypedArray or Array
                if (itemsTypedArrayBase)
                {
                    kValue = itemsTypedArrayBase->DirectGetItem(k);
                }
                else if (itemsArray)
                {
                    kValue = itemsArray->DirectGetItem(k);
                }
                else
                {
                    kValue = JavascriptOperators::OP_GetElementI_UInt32(items, k, scriptContext);
                }

                if (mapping)
                {
                    Assert(mapFn != nullptr);
                    Assert(mapFnThisArg != nullptr);

                    Js::Var mapFnArgs[] = { mapFnThisArg, kValue, JavascriptNumber::ToVar(k, scriptContext) };
                    Js::CallInfo mapFnCallInfo(Js::CallFlags_Value, _countof(mapFnArgs));
                    kValue = mapFn->CallFunction(Js::Arguments(mapFnCallInfo, mapFnArgs));
                }

                // If constructor built a TypedArray (likely) or Array (maybe likely) we can do a more direct set operation
                if (newTypedArrayBase)
                {
                    newTypedArrayBase->DirectSetItem(k, kValue, false);
                }
                else if (newArr)
                {
                    newArr->SetItem(k, kValue, Js::PropertyOperation_ThrowIfNotExtensible);
                }
                else
                {
                    JavascriptOperators::OP_SetElementI_UInt32(newObj, k, kValue, scriptContext, Js::PropertyOperation_ThrowIfNotExtensible);
                }
            }
        }

        return newObj;
    }

    Var TypedArrayBase::EntryOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAOfCount);

        if (args.Info.Count < 1)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedFunction, L"[TypedArray].of");
        }

        return JavascriptArray::OfHelper(true, args, scriptContext);
    }

    Var TypedArrayBase::EntryCopyWithin(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TACopyWithinCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.copyWithin");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        uint32 length = typedArrayBase->GetLength();

        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.copyWithin");
        }

        return JavascriptArray::CopyWithinHelper(nullptr, typedArrayBase, typedArrayBase, length, args, scriptContext);
    }

    Var TypedArrayBase::EntryEntries(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAEntriesCount);

        if (args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"[TypedArray].prototype.entries");
        }

        RecyclableObject* thisObj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &thisObj))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"[TypedArray].prototype.entries");
        }

        if (TypedArrayBase::IsDetachedTypedArray(thisObj))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.entries");
        }

        return scriptContext->GetLibrary()->CreateArrayIterator(thisObj, JavascriptArrayIteratorKind::KeyAndValue);
    }

    Var TypedArrayBase::EntryEvery(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"[TypedArray].prototype.every");

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAEveryCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.every");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        uint32 length = typedArrayBase->GetLength();

        return JavascriptArray::EveryHelper(nullptr, typedArrayBase, typedArrayBase, length, args, scriptContext);
    }

    Var TypedArrayBase::EntryFill(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAFillCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (!TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.fill");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        uint32 length = typedArrayBase->GetLength();

        return JavascriptArray::FillHelper(nullptr, typedArrayBase, typedArrayBase, length, args, scriptContext);
    }

    // %TypedArray%.prototype.filter as described in ES6.0 (draft 22) Section 22.2.3.9
    Var TypedArrayBase::EntryFilter(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"[TypedArray].prototype.filter");

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAFilterCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (!TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.filter");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        uint32 length = typedArrayBase->GetLength();

        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, L"[TypedArray].prototype.filter");
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg = nullptr;

        if (args.Info.Count > 2)
        {
            thisArg = args[2];
        }
        else
        {
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // We won't construct the return object until after walking over the elements of the TypedArray
        Var constructor = JavascriptOperators::SpeciesConstructor(
            typedArrayBase, TypedArrayBase::GetDefaultConstructor(args[0], scriptContext), scriptContext);

        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        Var selected = nullptr;
        RecyclableObject* newObj = nullptr;

        DECLARE_TEMP_GUEST_ALLOCATOR(tempAlloc);

        ACQUIRE_TEMP_GUEST_ALLOCATOR(tempAlloc, scriptContext, L"Runtime");
        {
            // Create a temporary list to hold the items selected by the callback function.
            // We will then iterate over this list and append those items into the object we will return.
            // We have to collect the items into this temporary list because we need to call the
            // new object constructor with a length of items and we don't know what length will be
            // until we know how many items we will collect.
            JsUtil::List<Var, ArenaAllocator>* tempList = JsUtil::List<Var, ArenaAllocator>::New(tempAlloc);

            for (uint32 k = 0; k < length; k++)
            {
                // We know that the TypedArray.HasItem will be true because k < length and we can skip the check in the TypedArray version of filter.
                element = typedArrayBase->DirectGetItem(k);

                selected = callBackFn->GetEntryPoint()(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    typedArrayBase);

                if (JavascriptConversion::ToBoolean(selected, scriptContext))
                {
                    tempList->Add(element);
                }
            }

            uint32 captured = tempList->Count();

            Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(captured, scriptContext) };
            Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
            newObj = RecyclableObject::FromVar(JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext));

            if (TypedArrayBase::Is(newObj))
            {
                // We are much more likely to have a TypedArray here than anything else
                TypedArrayBase* newArr = TypedArrayBase::FromVar(newObj);

                for (uint32 i = 0; i < captured; i++)
                {
                    newArr->DirectSetItem(i, tempList->Item(i), false);
                }
            }
            else
            {
                for (uint32 i = 0; i < captured; i++)
                {
                    JavascriptOperators::OP_SetElementI_UInt32(newObj, i, tempList->Item(i), scriptContext, PropertyOperation_ThrowIfNotExtensible);
                }
            }
        }
        RELEASE_TEMP_GUEST_ALLOCATOR(tempAlloc, scriptContext);

        return newObj;
    }

    Var TypedArrayBase::EntryFind(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"[TypedArray].prototype.find");

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAFindCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.find");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.find");
        }
        uint32 length = typedArrayBase->GetLength();

        return JavascriptArray::FindHelper<false>(nullptr, typedArrayBase, typedArrayBase, length, args, scriptContext);
    }

    Var TypedArrayBase::EntryFindIndex(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"[TypedArray].prototype.findIndex");

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAFindIndexCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.findIndex");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.findIndex");
        }
        uint32 length = typedArrayBase->GetLength();

        return JavascriptArray::FindHelper<true>(nullptr, typedArrayBase, typedArrayBase, length, args, scriptContext);
    }

    // %TypedArray%.prototype.forEach as described in ES6.0 (draft 22) Section 22.2.3.12
    Var TypedArrayBase::EntryForEach(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"[TypedArray].prototype.forEach");

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAForEachCount);

        uint32 length;

        if (args.Info.Count < 1 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.forEach");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.forEach");
        }
        length = typedArrayBase->GetLength();

        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, L"[TypedArray].prototype.forEach");
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg;

        if (args.Info.Count > 2)
        {
            thisArg = args[2];
        }
        else
        {
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        Var element;
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;

        for (uint32 k = 0; k < length; k++)
        {
            // We know the TypedArray has this item (and a proxy can't lie to us about that) but there might be a proxy trap on HasProperty
            // so we still have to call the function.
            typedArrayBase->HasItem(k);

            element = typedArrayBase->DirectGetItem(k);

            callBackFn->GetEntryPoint()(callBackFn, CallInfo(flags, 4), thisArg,
                element,
                JavascriptNumber::ToVar(k, scriptContext),
                typedArrayBase);
        }

        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var TypedArrayBase::EntryIndexOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAIndexOfCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (!TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.indexOf");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.indexOf");
        }
        uint32 length = typedArrayBase->GetLength();

        Var search;
        uint32 fromIndex;
        if (!JavascriptArray::GetParamForIndexOf(length, args, search, fromIndex, scriptContext))
        {
            return TaggedInt::ToVarUnchecked(-1);
        }

        return JavascriptArray::TemplatedIndexOfHelper<false>(typedArrayBase, search, fromIndex, length, scriptContext);
    }

    Var TypedArrayBase::EntryIncludes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAIncludesCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (!TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.includes");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.includes");
        }
        uint32 length = typedArrayBase->GetLength();

        Var search;
        uint32 fromIndex;
        if (!JavascriptArray::GetParamForIndexOf(length, args, search, fromIndex, scriptContext))
        {
            return scriptContext->GetLibrary()->GetFalse();
        }

        return JavascriptArray::TemplatedIndexOfHelper<true>(typedArrayBase, search, fromIndex, length, scriptContext);
    }


    Var TypedArrayBase::EntryJoin(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAJoinCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count < 1 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.join");
        }

        JavascriptLibrary* library = scriptContext->GetLibrary();
        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.join");
        }
        uint32 length = typedArrayBase->GetLength();
        JavascriptString* separator = nullptr;

        if (length == 0)
        {
            return library->GetEmptyString();
        }
        else if (length == 1)
        {
            return JavascriptConversion::ToString(typedArrayBase->DirectGetItem(0), scriptContext);
        }

        if (args.Info.Count > 1)
        {
            separator = JavascriptConversion::ToString(args[1], scriptContext);
        }
        else
        {
            separator = library->GetCommaDisplayString();
        }

        bool hasSeparator = (separator->GetLength() != 0);

        charcount_t estimatedAppendSize = min(
            static_cast<charcount_t>((64 << 20) / sizeof(void *)), // 64 MB worth of pointers
            static_cast<charcount_t>(length + (hasSeparator ? length - 1 : 0)));

        CompoundString* const cs = CompoundString::NewWithPointerCapacity(estimatedAppendSize, library);

        Assert(length >= 2);

        JavascriptString* element = JavascriptConversion::ToString(typedArrayBase->DirectGetItem(0), scriptContext);

        cs->Append(element);

        for (uint32 i = 1; i < length; i++)
        {
            if (hasSeparator)
            {
                cs->Append(separator);
            }

            // Since i < length, we can be certain that the TypedArray contains an item at index i and we don't have to check for undefined
            element = JavascriptConversion::ToString(typedArrayBase->DirectGetItem(i), scriptContext);

            cs->Append(element);
        }

        return cs;
    }

    Var TypedArrayBase::EntryKeys(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAKeysCount);

        if (args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"[TypedArray].prototype.keys");
        }

        RecyclableObject* thisObj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &thisObj))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"[TypedArray].prototype.keys");
        }

        if (TypedArrayBase::IsDetachedTypedArray(thisObj))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.keys");
        }

        return scriptContext->GetLibrary()->CreateArrayIterator(thisObj, JavascriptArrayIteratorKind::Key);
    }

    Var TypedArrayBase::EntryLastIndexOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TALastIndexOfCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count < 1 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.lastIndexOf");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.lastIndexOf");
        }
        uint32 length = typedArrayBase->GetLength();

        Var search;
        int64 fromIndex;
        if (!JavascriptArray::GetParamForLastIndexOf(length, args, search, fromIndex, scriptContext))
        {
            return TaggedInt::ToVarUnchecked(-1);
        }

        return JavascriptArray::LastIndexOfHelper(typedArrayBase, search, fromIndex, scriptContext);
    }

    Var TypedArrayBase::EntryMap(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"[TypedArray].prototype.map");

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAMapCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.map");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.map");
        }
        uint32 length = typedArrayBase->GetLength();

        return JavascriptArray::MapHelper(nullptr, typedArrayBase, typedArrayBase, length, args, scriptContext);
    }

    Var TypedArrayBase::EntryReduce(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"[TypedArray].prototype.reduce");

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAReduceCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.reduce");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.reduce");
        }
        uint32 length = typedArrayBase->GetLength();

        return JavascriptArray::ReduceHelper(nullptr, typedArrayBase, typedArrayBase, length, args, scriptContext);
    }

    Var TypedArrayBase::EntryReduceRight(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"[TypedArray].prototype.reduceRight");

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAReduceRightCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.reduceRight");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.reduceRight");
        }
        uint32 length = typedArrayBase->GetLength();

        return JavascriptArray::ReduceRightHelper(nullptr, typedArrayBase, typedArrayBase, length, args, scriptContext);
    }

    Var TypedArrayBase::EntryReverse(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAReverseCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.reverse");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.reverse");
        }
        uint32 length = typedArrayBase->GetLength();

        return JavascriptArray::ReverseHelper(nullptr, typedArrayBase, typedArrayBase, length, scriptContext);
    }

    Var TypedArrayBase::EntrySlice(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.slice");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.slice");
        }
        uint32 length = typedArrayBase->GetLength();

        return JavascriptArray::SliceHelper(nullptr, typedArrayBase, typedArrayBase, length, args, scriptContext);
    }

    Var TypedArrayBase::EntrySome(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"[TypedArray].prototype.some");

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TASomeCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.some");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.some");
        }
        uint32 length = typedArrayBase->GetLength();

        return JavascriptArray::SomeHelper(nullptr, typedArrayBase, typedArrayBase, length, args, scriptContext);
    }

    template<typename T> int __cdecl TypedArrayCompareElementsHelper(void* context, const void* elem1, const void* elem2)
    {
        const T* element1 = static_cast<const T*>(elem1);
        const T* element2 = static_cast<const T*>(elem2);

        Assert(element1 != nullptr);
        Assert(element2 != nullptr);
        Assert(context != nullptr);

        const T x = *element1;
        const T y = *element2;

        if (NumberUtilities::IsNan((double)x))
        {
            if (NumberUtilities::IsNan((double)y))
            {
                return 0;
            }

            return 1;
        }
        else
        {
            if (NumberUtilities::IsNan((double)y))
            {
                return -1;
            }
        }

        void **contextArray = (void **)context;
        if (contextArray[1] != nullptr)
        {
            RecyclableObject* compFn = RecyclableObject::FromVar(contextArray[1]);
            ScriptContext* scriptContext = compFn->GetScriptContext();
            Var undefined = scriptContext->GetLibrary()->GetUndefined();
            double dblResult;
            Var retVal = compFn->GetEntryPoint()(compFn, CallInfo(CallFlags_Value, 3),
                undefined,
                JavascriptNumber::ToVarWithCheck((double)x, scriptContext),
                JavascriptNumber::ToVarWithCheck((double)y, scriptContext));

            Assert(TypedArrayBase::Is(contextArray[0]));
            if (TypedArrayBase::IsDetachedTypedArray(contextArray[0]))
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.sort");
            }

            if (TaggedInt::Is(retVal))
            {
                return TaggedInt::ToInt32(retVal);
            }

            if (JavascriptNumber::Is_NoTaggedIntCheck(retVal))
            {
                dblResult = JavascriptNumber::GetValue(retVal);
            }
            else
            {
                dblResult = JavascriptConversion::ToNumber_Full(retVal, scriptContext);
            }

            if (dblResult < 0)
            {
                return -1;
            }
            else if (dblResult > 0)
            {
                return 1;
            }

            return 0;
        }
        else
        {
            if (x < y)
            {
                return -1;
            }
            else if (x > y)
            {
                return 1;
            }

            return 0;
        }
    }

    Var TypedArrayBase::EntrySort(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"[TypedArray].prototype.sort");

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TASortCount);

        // For the TypedArray version of the API, we need to throw immediately if this is not a TypedArray object
        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray, L"[TypedArray].prototype.sort");
        }

        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        uint32 length = typedArrayBase->GetLength();

        if (typedArrayBase->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.sort");
        }

        // If TypedArray has no length, we don't have any work to do.
        if (length == 0)
        {
            return typedArrayBase;
        }

        RecyclableObject* compareFn = nullptr;

        if (args.Info.Count > 1)
        {
            if (!JavascriptConversion::IsCallable(args[1]))
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, L"[TypedArray].prototype.sort");
            }

            compareFn = RecyclableObject::FromVar(args[1]);
        }

        // Get the elements comparison function for the type of this TypedArray
        void* elementCompare = typedArrayBase->GetCompareElementsFunction();

        Assert(elementCompare);

        // Cast compare to the correct function type
        int(__cdecl*elementCompareFunc)(void*, const void*, const void*) = (int(__cdecl*)(void*, const void*, const void*))elementCompare;

        void * contextToPass[] = { typedArrayBase, compareFn };

        // We can always call qsort_s with the same arguments. If user compareFn is non-null, the callback will use it to do the comparison.
        qsort_s(typedArrayBase->GetByteBuffer(), length, typedArrayBase->GetBytesPerElement(), elementCompareFunc, contextToPass);


        return typedArrayBase;
    }

    Var TypedArrayBase::EntryValues(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TAValuesCount);

        if (args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"[TypedArray].prototype.values");
        }

        RecyclableObject* thisObj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &thisObj))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"[TypedArray].prototype.values");
        }

        if (TypedArrayBase::IsDetachedTypedArray(thisObj))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"[TypedArray].prototype.values");
        }

        return scriptContext->GetLibrary()->CreateArrayIterator(thisObj, JavascriptArrayIteratorKind::Value);
    }

    BOOL TypedArrayBase::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        ENTER_PINNED_SCOPE(JavascriptString, toStringResult);
        toStringResult = JavascriptObject::ToStringInternal(this, requestContext);
        stringBuilder->Append(toStringResult->GetString(), toStringResult->GetLength());
        LEAVE_PINNED_SCOPE();
        return TRUE;
    }

    BOOL TypedArrayBase::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        switch(GetTypeId())
        {
        case TypeIds_Int8Array:
            stringBuilder->AppendCppLiteral(L"Object, (Int8Array)");
            break;

        case TypeIds_Uint8Array:
            stringBuilder->AppendCppLiteral(L"Object, (Uint8Array)");
            break;

        case TypeIds_Uint8ClampedArray:
            stringBuilder->AppendCppLiteral(L"Object, (Uint8ClampedArray)");
            break;

        case TypeIds_Int16Array:
            stringBuilder->AppendCppLiteral(L"Object, (Int16Array)");
            break;

        case TypeIds_Uint16Array:
            stringBuilder->AppendCppLiteral(L"Object, (Uint16Array)");
            break;

        case TypeIds_Int32Array:
            stringBuilder->AppendCppLiteral(L"Object, (Int32Array)");
            break;

        case TypeIds_Uint32Array:
            stringBuilder->AppendCppLiteral(L"Object, (Uint32Array)");
            break;

        case TypeIds_Float32Array:
            stringBuilder->AppendCppLiteral(L"Object, (Float32Array)");
            break;

        case TypeIds_Float64Array:
            stringBuilder->AppendCppLiteral(L"Object, (Float64Array)");
            break;

        default:
            Assert(false);
            stringBuilder->AppendCppLiteral(L"Object");
            break;
        }

        return TRUE;
    }

    bool TypedArrayBase::TryGetLengthForOptimizedTypedArray(const Var var, uint32 *const lengthRef, TypeId *const typeIdRef)
    {
        Assert(var);
        Assert(lengthRef);
        Assert(typeIdRef);

        if(!RecyclableObject::Is(var))
        {
            return false;
        }

        const TypeId typeId = RecyclableObject::FromVar(var)->GetTypeId();
        switch(typeId)
        {
            case TypeIds_Int8Array:
            case TypeIds_Uint8Array:
            case TypeIds_Uint8ClampedArray:
            case TypeIds_Int16Array:
            case TypeIds_Uint16Array:
            case TypeIds_Int32Array:
            case TypeIds_Uint32Array:
            case TypeIds_Float32Array:
            case TypeIds_Float64Array:
                Assert(ValueType::FromTypeId(typeId,false).IsOptimizedTypedArray());
                *lengthRef = FromVar(var)->GetLength();
                *typeIdRef = typeId;
                return true;
        }

        Assert(!ValueType::FromTypeId(typeId,false).IsOptimizedTypedArray());
        return false;
    }

    BOOL TypedArrayBase::ValidateIndexAndDirectSetItem(__in Js::Var index, __in Js::Var value, __in bool * isNumericIndex)
    {
        bool skipSetItem = false;
        uint32 indexToSet = ValidateAndReturnIndex(index, &skipSetItem, isNumericIndex);

        // If index is not numeric, goto [[Set]] property path
        if (*isNumericIndex)
            return DirectSetItem(indexToSet, value, skipSetItem);
        else
            return TRUE;
    }

    // Validate the index used for typed arrays with below rules:
    // 1. if numeric string, let sIndex = ToNumber(index) :
    //    a. if sIndex is not integer, skip set operation
    //    b. if sIndex == -0, skip set operation
    //    c. if sIndex < 0 or sIndex >= length, skip set operation
    //    d. else return sIndex and perform set operation
    // 2. if tagged int, let nIndex = untag(index) :
    //    a. if nIndex < 0 or nIndex >= length, skip set operation
    // 3. else:
    //    a. if index is not integer, skip set operation
    //    b. if index < 0 or index >= length, skip set operation
    //    NOTE: if index == -0, it is treated as 0 and perform set operation
    //          as per 7.1.12.1 of ES6 spec 7.1.12.1 ToString Applied to the Number Type
    uint32 TypedArrayBase::ValidateAndReturnIndex(__in Js::Var index, __in bool * skipOperation, __in bool * isNumericIndex)
    {
        *skipOperation = false;
        *isNumericIndex = true;
        uint32 length = GetLength();

        if (TaggedInt::Is(index))
        {
            int32 indexInt = TaggedInt::ToInt32(index);
            *skipOperation = (indexInt < 0 || (uint32)indexInt >= length);
            return (uint32)indexInt;
        }
        else
        {
            double dIndexValue = 0;
            if (JavascriptString::Is(index))
            {
                if (JavascriptConversion::CanonicalNumericIndexString(index, &dIndexValue, GetScriptContext()))
                {
                    if (JavascriptNumber::IsNegZero(dIndexValue))
                    {
                        *skipOperation = true;
                        return 0;
                    }
                    // If this is numeric index embedded in string, perform regular numeric index checks below
                }
                else
                {
                    // not numeric index, go the [[Set]] path to add as string property
                    *isNumericIndex = false;
                    return 0;
                }
            }
            else
            {
                // JavascriptNumber::Is_NoTaggedIntCheck(index)
                dIndexValue = JavascriptNumber::GetValue(index);
            }

            // OK to lose data because we want to verify ToInteger()
            uint32 uint32Index = (uint32)dIndexValue;

            // IsInteger()
            if ((double)uint32Index != dIndexValue)
            {
                *skipOperation = true;
            }
            // index >= length
            else if (uint32Index >= GetLength())
            {
                *skipOperation = true;
            }
            return uint32Index;
        }
    }

    // static
    Var TypedArrayBase::GetDefaultConstructor(Var object, ScriptContext* scriptContext)
    {
        TypeId typeId = JavascriptOperators::GetTypeId(object);
        Var defaultConstructor = nullptr;
        switch (typeId)
        {
        case TypeId::TypeIds_Int8Array:
            defaultConstructor = scriptContext->GetLibrary()->GetInt8ArrayConstructor();
            break;
        case TypeId::TypeIds_Uint8Array:
            defaultConstructor = scriptContext->GetLibrary()->GetUint8ArrayConstructor();
            break;
        case TypeId::TypeIds_Uint8ClampedArray:
            defaultConstructor = scriptContext->GetLibrary()->GetUint8ClampedArrayConstructor();
            break;
        case TypeId::TypeIds_Int16Array:
            defaultConstructor = scriptContext->GetLibrary()->GetInt16ArrayConstructor();
            break;
        case TypeId::TypeIds_Uint16Array:
            defaultConstructor = scriptContext->GetLibrary()->GetUint16ArrayConstructor();
            break;
        case TypeId::TypeIds_Int32Array:
            defaultConstructor = scriptContext->GetLibrary()->GetInt32ArrayConstructor();
            break;
        case TypeId::TypeIds_Uint32Array:
            defaultConstructor = scriptContext->GetLibrary()->GetUint32ArrayConstructor();
            break;
        case TypeId::TypeIds_Float32Array:
            defaultConstructor = scriptContext->GetLibrary()->GetFloat32ArrayConstructor();
            break;
        case TypeId::TypeIds_Float64Array:
            defaultConstructor = scriptContext->GetLibrary()->GetFloat32ArrayConstructor();
            break;
        default:
            Assert(false);
        }
        return defaultConstructor;
    }

    Var TypedArrayBase::FindMinOrMax(Js::ScriptContext * scriptContext, TypeId typeId, bool findMax)
    {
        if (this->IsDetachedBuffer()) // 9.4.5.8 IntegerIndexedElementGet
        {
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_DetachedTypedArray);
        }

        switch (typeId)
        {
        case TypeIds_Int8Array:
            return this->FindMinOrMax<int8, false>(scriptContext, findMax);

        case TypeIds_Uint8Array:
        case TypeIds_Uint8ClampedArray:
            return this->FindMinOrMax<uint8, false>(scriptContext, findMax);

        case TypeIds_Int16Array:
            return this->FindMinOrMax<int16, false>(scriptContext, findMax);

        case TypeIds_Uint16Array:
            return this->FindMinOrMax<uint16, false>(scriptContext, findMax);

        case TypeIds_Int32Array:
            return this->FindMinOrMax<int32, false>(scriptContext, findMax);

        case TypeIds_Uint32Array:
            return this->FindMinOrMax<uint32, false>(scriptContext, findMax);

        case TypeIds_Float32Array:
            return this->FindMinOrMax<float, true>(scriptContext, findMax);

        case TypeIds_Float64Array:
            return this->FindMinOrMax<double, true>(scriptContext, findMax);

        default:
            AssertMsg(false, "Unsupported array for fast path");
            return nullptr;
        }
    }

    template<typename T, bool checkNaNAndNegZero>
    Var TypedArrayBase::FindMinOrMax(Js::ScriptContext * scriptContext, bool findMax)
    {
        T* typedBuffer = (T*)this->buffer;
        uint len = this->GetLength();

        Assert(sizeof(T)+GetByteOffset() <= GetArrayBuffer()->GetByteLength());
        T currentRes = typedBuffer[0];
        for (uint i = 0; i < len; i++)
        {
            Assert((i + 1) * sizeof(T)+GetByteOffset() <= GetArrayBuffer()->GetByteLength());
            T compare = typedBuffer[i];
            if (checkNaNAndNegZero && JavascriptNumber::IsNan(double(compare)))
            {
                return scriptContext->GetLibrary()->GetNaN();
            }
            if (findMax ? currentRes < compare : currentRes > compare ||
                (checkNaNAndNegZero && compare == 0 && Js::JavascriptNumber::IsNegZero(double(currentRes))))
            {
                currentRes = compare;
            }
        }
        return Js::JavascriptNumber::ToVarNoCheck(currentRes, scriptContext);
    }

    template<> BOOL Uint8ClampedArray::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint8ClampedArray &&
               ( VirtualTableInfo<Uint8ClampedArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint8ClampedArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint8Array::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint8Array &&
              ( VirtualTableInfo<Uint8Array>::HasVirtualTable(aValue) ||
                VirtualTableInfo<CrossSiteObject<Uint8Array>>::HasVirtualTable(aValue)
              );
    }

    template<> BOOL Int8Array::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int8Array &&
               ( VirtualTableInfo<Int8Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int8Array>>::HasVirtualTable(aValue)
               );
    }


    template<> BOOL Int16Array::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int16Array &&
               ( VirtualTableInfo<Int16Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int16Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint16Array::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint16Array &&
              ( VirtualTableInfo<Uint16Array>::HasVirtualTable(aValue) ||
                VirtualTableInfo<CrossSiteObject<Uint16Array>>::HasVirtualTable(aValue)
              );
    }

    template<> BOOL Int32Array::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int32Array &&
               ( VirtualTableInfo<Int32Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int32Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint32Array::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint32Array &&
               ( VirtualTableInfo<Uint32Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint32Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Float32Array::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Float32Array &&
               ( VirtualTableInfo<Float32Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Float32Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Float64Array::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Float64Array &&
               ( VirtualTableInfo<Float64Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Float64Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Int64Array::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int64Array &&
               ( VirtualTableInfo<Int64Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int64Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint64Array::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint64Array &&
               ( VirtualTableInfo<Uint64Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint64Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL BoolArray::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_BoolArray &&
               ( VirtualTableInfo<BoolArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<BoolArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint8ClampedVirtualArray::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint8ClampedArray &&
               ( VirtualTableInfo<Uint8ClampedVirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint8ClampedVirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint8VirtualArray::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint8Array &&
               ( VirtualTableInfo<Uint8VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint8VirtualArray>>::HasVirtualTable(aValue)
               );
    }


    template<> BOOL Int8VirtualArray::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int8Array &&
               ( VirtualTableInfo<Int8VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int8VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Int16VirtualArray::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int16Array &&
               ( VirtualTableInfo<Int16VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int16VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint16VirtualArray::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint16Array &&
               ( VirtualTableInfo<Uint16VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint16VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Int32VirtualArray::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int32Array &&
               ( VirtualTableInfo<Int32VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int32VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint32VirtualArray::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint32Array &&
               ( VirtualTableInfo<Uint32VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint32VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Float32VirtualArray::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Float32Array &&
               ( VirtualTableInfo<Float32VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Float32VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Float64VirtualArray::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Float64Array &&
               ( VirtualTableInfo<Float64VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Float64VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template <typename TypeName, bool clamped, bool virtualAllocated>
    TypedArray<TypeName, clamped, virtualAllocated>* TypedArray<TypeName, clamped, virtualAllocated>::FromVar(Var aValue)
    {
        AssertMsg(TypedArray::Is(aValue), "invalid TypedArray");
        return static_cast<TypedArray<TypeName, clamped, virtualAllocated>*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint8ClampedArray* Uint8ClampedArray::FromVar(Var aValue)
    {
        AssertMsg(Uint8ClampedArray::Is(aValue), "invalid Uint8ClampedArray");
        return static_cast<Uint8ClampedArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint8Array* Uint8Array::FromVar(Var aValue)
    {
        AssertMsg(Uint8Array::Is(aValue), "invalid Uint8Array");
        return static_cast<Uint8Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int8Array* Int8Array::FromVar(Var aValue)
    {
        AssertMsg(Int8Array::Is(aValue), "invalid Int8Array");
        return static_cast<Int8Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int16Array* Int16Array::FromVar(Var aValue)
    {
        AssertMsg(Int16Array::Is(aValue), "invalid Int16Array");
        return static_cast<Int16Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint16Array* Uint16Array::FromVar(Var aValue)
    {
        AssertMsg(Uint16Array::Is(aValue), "invalid Uint16Array");
        return static_cast<Uint16Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int32Array* Int32Array::FromVar(Var aValue)
    {
        AssertMsg(Int32Array::Is(aValue), "invalid Int32Array");
        return static_cast<Int32Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint32Array* Uint32Array::FromVar(Var aValue)
    {
        AssertMsg(Uint32Array::Is(aValue), "invalid Uint32Array");
        return static_cast<Uint32Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Float32Array* Float32Array::FromVar(Var aValue)
    {
        AssertMsg(Float32Array::Is(aValue), "invalid Float32Array");
        return static_cast<Float32Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Float64Array* Float64Array::FromVar(Var aValue)
    {
        AssertMsg(Float64Array::Is(aValue), "invalid Float64Array");
        return static_cast<Float64Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int64Array* Int64Array::FromVar(Var aValue)
    {
        AssertMsg(Int64Array::Is(aValue), "invalid Int64Array");
        return static_cast<Int64Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint64Array* Uint64Array::FromVar(Var aValue)
    {
        AssertMsg(Uint64Array::Is(aValue), "invalid Uint64Array");
        return static_cast<Uint64Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int8VirtualArray* Int8VirtualArray::FromVar(Var aValue)
    {
        AssertMsg(Int8VirtualArray::Is(aValue), "invalid Int8Array");
        return static_cast<Int8VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint8ClampedVirtualArray* Uint8ClampedVirtualArray::FromVar(Var aValue)
    {
        AssertMsg(Uint8ClampedVirtualArray::Is(aValue), "invalid Uint8ClampedArray");
        return static_cast<Uint8ClampedVirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint8VirtualArray* Uint8VirtualArray::FromVar(Var aValue)
    {
        AssertMsg(Uint8VirtualArray::Is(aValue), "invalid Uint8Array");
        return static_cast<Uint8VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int16VirtualArray* Int16VirtualArray::FromVar(Var aValue)
    {
        AssertMsg(Int16VirtualArray::Is(aValue), "invalid Int16Array");
        return static_cast<Int16VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint16VirtualArray* Uint16VirtualArray::FromVar(Var aValue)
    {
        AssertMsg(Uint16VirtualArray::Is(aValue), "invalid Uint16Array");
        return static_cast<Uint16VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int32VirtualArray* Int32VirtualArray::FromVar(Var aValue)
    {
        AssertMsg(Int32VirtualArray::Is(aValue), "invalid Int32Array");
        return static_cast<Int32VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint32VirtualArray* Uint32VirtualArray::FromVar(Var aValue)
    {
        AssertMsg(Uint32VirtualArray::Is(aValue), "invalid Uint32Array");
        return static_cast<Uint32VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Float32VirtualArray* Float32VirtualArray::FromVar(Var aValue)
    {
        AssertMsg(Float32VirtualArray::Is(aValue), "invalid Float32Array");
        return static_cast<Float32VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Float64VirtualArray* Float64VirtualArray::FromVar(Var aValue)
    {
        AssertMsg(Float64VirtualArray::Is(aValue), "invalid Float64Array");
        return static_cast<Float64VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> BoolArray* BoolArray::FromVar(Var aValue)
    {
        AssertMsg(BoolArray::Is(aValue), "invalid BoolArray");
        return static_cast<BoolArray*>(RecyclableObject::FromVar(aValue));
    }

    __inline BOOL Int8Array::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToInt8);
    }

    __inline BOOL Int8VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToInt8);
    }

    __inline Var Int8Array::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }

    __inline Var Int8VirtualArray::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }

    __inline BOOL Uint8Array::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToUInt8);
    }
    __inline BOOL Uint8VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToUInt8);
    }

    __inline Var Uint8Array::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }
    __inline Var Uint8VirtualArray::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }


    __inline BOOL Uint8ClampedArray::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToUInt8Clamped);
    }

    __inline Var Uint8ClampedArray::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }

    __inline BOOL Uint8ClampedVirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToUInt8Clamped);
    }

    __inline Var Uint8ClampedVirtualArray::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }

    __inline BOOL Int16Array::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToInt16);
    }

    __inline Var Int16Array::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }

    __inline BOOL Int16VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToInt16);
    }

    __inline Var Int16VirtualArray::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }


    __inline BOOL Uint16Array::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToUInt16);
    }

    __inline Var Uint16Array::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }

    __inline BOOL Uint16VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToUInt16);
    }

    __inline Var Uint16VirtualArray::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }



    __inline BOOL Int32Array::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToInt32);
    }

    __inline Var Int32Array::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }

    __inline BOOL Int32VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToInt32);
    }

    __inline Var Int32VirtualArray::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }


    __inline BOOL Uint32Array::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToUInt32);
    }

    __inline Var Uint32Array::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }

    __inline BOOL Uint32VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToUInt32);
    }

    __inline Var Uint32VirtualArray::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }



    __inline BOOL Float32Array::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToFloat);
    }

    Var Float32Array::DirectGetItem(__in uint32 index)
    {
        return TypedDirectGetItemWithCheck(index);
    }

    __inline BOOL Float32VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToFloat);
    }

    Var Float32VirtualArray::DirectGetItem(__in uint32 index)
    {
        return TypedDirectGetItemWithCheck(index);
    }


    __inline BOOL Float64Array::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToNumber);
    }

    __inline Var Float64Array::DirectGetItem(__in uint32 index)
    {
        return TypedDirectGetItemWithCheck(index);
    }

    __inline BOOL Float64VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToNumber);
    }

    __inline Var Float64VirtualArray::DirectGetItem(__in uint32 index)
    {
        return TypedDirectGetItemWithCheck(index);
    }


    __inline BOOL Int64Array::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToInt64);
    }

    __inline Var Int64Array::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }

    __inline BOOL Uint64Array::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToUInt64);
    }

    __inline Var Uint64Array::DirectGetItem(__in uint32 index)
    {
        return BaseTypedDirectGetItem(index);
    }

    __inline BOOL BoolArray::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        return BaseTypedDirectSetItem(index, value, skipSetElement, JavascriptConversion::ToBool);
    }

    __inline Var BoolArray::DirectGetItem(__in uint32 index)
    {
        if (index < GetLength())
        {
            Assert((index+1)* sizeof(bool) +GetByteOffset() <= GetArrayBuffer()->GetByteLength());
            bool* typedBuffer = (bool*)buffer;
            return typedBuffer[index] ? GetLibrary()->GetTrue() : GetLibrary()->GetFalse();
        }
        return GetLibrary()->GetUndefined();
    }

    Var CharArray::Create(ArrayBuffer* arrayBuffer, uint32 byteOffSet, uint32 mappedLength, JavascriptLibrary* javascirptLibrary)
    {
        CharArray* arr;
        uint32 totalLength, mappedByteLength;
        if (UInt32Math::Mul(mappedLength, sizeof(wchar_t), &mappedByteLength) ||
            UInt32Math::Add(byteOffSet, mappedByteLength, &totalLength) ||
            (totalLength > arrayBuffer->GetByteLength()))
        {
            JavascriptError::ThrowRangeError(arrayBuffer->GetScriptContext(), JSERR_InvalidTypedArrayLength);
        }
        arr = RecyclerNew(javascirptLibrary->GetRecycler(), CharArray, arrayBuffer, byteOffSet, mappedLength, javascirptLibrary->GetCharArrayType());
        return arr;
    }

    BOOL CharArray::Is(Var value)
    {
        return JavascriptOperators::GetTypeId(value) == Js::TypeIds_CharArray;
    }

    Var CharArray::EntrySet(RecyclableObject* function, CallInfo callInfo, ...)
    {
        AssertMsg(FALSE, "not supported in char array");
        JavascriptError::ThrowTypeError(function->GetScriptContext(), JSERR_This_NeedTypedArray);
    }

    Var CharArray::EntrySubarray(RecyclableObject* function, CallInfo callInfo, ...)
    {
        AssertMsg(FALSE, "not supported in char array");
        JavascriptError::ThrowTypeError(function->GetScriptContext(), JSERR_This_NeedTypedArray);
    }

    __inline Var CharArray::Subarray(uint32 begin, uint32 end)
    {
        AssertMsg(FALSE, "not supported in char array");
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_This_NeedTypedArray);
    }

    __inline CharArray* CharArray::FromVar(Var aValue)
    {
        AssertMsg(CharArray::Is(aValue), "invalid CharArray");
        return static_cast<CharArray*>(RecyclableObject::FromVar(aValue));
    }

    __inline BOOL CharArray::DirectSetItem(__in uint32 index, __in Js::Var value, __in bool skipSetElement)
    {
        ScriptContext* scriptContext = GetScriptContext();
        // A typed array is Integer Indexed Exotic object, so doing a get translates to 9.4.5.9 IntegerIndexedElementSet
        LPCWSTR asString = (Js::JavascriptConversion::ToString(value, scriptContext))->GetSz();

        if (this->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray);
        }

        if (skipSetElement)
        {
            return FALSE;
        }

        AssertMsg(index < GetLength(), "Trying to set out of bound index for typed array.");
        Assert((index + 1)* sizeof(wchar_t) + GetByteOffset() <= GetArrayBuffer()->GetByteLength());
        wchar_t* typedBuffer = (wchar_t*)buffer;

        if (asString != NULL && ::wcslen(asString) == 1)
        {
            typedBuffer[index] = asString[0];
        }
        else
        {
            Js::JavascriptError::MapAndThrowError(scriptContext, E_INVALIDARG);
        }

        return TRUE;
    }

    __inline Var CharArray::DirectGetItem(__in uint32 index)
    {
        // A typed array is Integer Indexed Exotic object, so doing a get translates to 9.4.5.8 IntegerIndexedElementGet
        if (this->IsDetachedBuffer())
        {
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_DetachedTypedArray);
        }
        if (index < GetLength())
        {
            Assert((index + 1)* sizeof(wchar_t)+GetByteOffset() <= GetArrayBuffer()->GetByteLength());
            wchar_t* typedBuffer = (wchar_t*)buffer;
            return GetLibrary()->GetCharStringCache().GetStringForChar(typedBuffer[index]);
        }
        return GetLibrary()->GetUndefined();
    }

    Var CharArray::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        function->GetScriptContext()->GetThreadContext()->ProbeStack(Js::Constants::MinStackDefault, function->GetScriptContext());

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Assert(!(callInfo.Flags & CallFlags_New) || args[0] == nullptr);
        Var object = TypedArrayBase::CreateNewInstance(args, scriptContext, sizeof(wchar_t), CharArray::Create);
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {
            object = Js::JavascriptProxy::AutoProxyWrapper(object);
        }
#endif
        return object;
    }
}

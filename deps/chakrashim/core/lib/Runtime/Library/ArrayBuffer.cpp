//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    PropertyId const ArrayBuffer::specialPropertyIds[] =
    {
        PropertyIds::byteLength
    };

    ArrayBuffer* ArrayBuffer::NewFromDetachedState(DetachedStateBase* state, JavascriptLibrary *library)
    {
        ArrayBufferDetachedStateBase* arrayBufferState = (ArrayBufferDetachedStateBase *)state;
        ArrayBuffer *toReturn = nullptr;

        switch (arrayBufferState->allocationType)
        {
        case ArrayBufferAllocationType::CoTask:
            toReturn = library->CreateProjectionArraybuffer(arrayBufferState->buffer, arrayBufferState->bufferLength);
            break;
        case ArrayBufferAllocationType::Heap:
        case ArrayBufferAllocationType::MemAlloc:
            toReturn = library->CreateArrayBuffer(arrayBufferState->buffer, arrayBufferState->bufferLength);
            break;
        default:
            AssertMsg(false, "Unknown allocationType of ArrayBufferDetachedStateBase ");
        }

        return toReturn;
    }

    void ArrayBuffer::ClearParentsLength(ArrayBufferParent* parent)
    {
        if (parent == nullptr)
        {
            return;
        }

        switch (JavascriptOperators::GetTypeId(parent))
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
            case TypeIds_Int64Array:
            case TypeIds_Uint64Array:
            case TypeIds_CharArray:
            case TypeIds_BoolArray:
                TypedArrayBase::FromVar(parent)->length = 0;
                break;

            case TypeIds_DataView:
                DataView::FromVar(parent)->length = 0;
                break;

            default:
                AssertMsg(false, "We need an explicit case for any parent of ArrayBuffer.");
                break;
        }
    }

    ArrayBufferDetachedStateBase* ArrayBuffer::DetachAndGetState()
    {
        Assert(!this->isDetached);

        AutoPtr<ArrayBufferDetachedStateBase> arrayBufferState(this->CreateDetachedState(this->buffer, this->bufferLength));

        this->buffer = nullptr;
        this->bufferLength = 0;
        this->isDetached = true;

        if (this->primaryParent != nullptr && this->primaryParent->Get() == nullptr)
        {
            this->primaryParent = nullptr;
        }

        if (this->primaryParent != nullptr)
        {
            this->ClearParentsLength(this->primaryParent->Get());
        }

        if (this->otherParents != nullptr)
        {
            this->otherParents->Map([&](int index, RecyclerWeakReference<ArrayBufferParent>* item)
            {
                this->ClearParentsLength(item->Get());
            });
        }

        return arrayBufferState.Detach();
    }

    void ArrayBuffer::AddParent(ArrayBufferParent* parent)
    {
        if (this->primaryParent == nullptr || this->primaryParent->Get() == nullptr)
        {
            this->primaryParent = this->GetRecycler()->CreateWeakReferenceHandle(parent);
        }
        else
        {
            if (this->otherParents == nullptr)
            {
                this->otherParents = JsUtil::List<RecyclerWeakReference<ArrayBufferParent>*>::New(this->GetRecycler());
            }
            this->otherParents->Add(this->GetRecycler()->CreateWeakReferenceHandle(parent));
        }
    }

    void ArrayBuffer::RemoveParent(ArrayBufferParent* parent)
    {
        if (this->primaryParent != nullptr && this->primaryParent->Get() == parent)
        {
            this->primaryParent = nullptr;
        }
        else
        {
            int foundIndex = -1;
            bool parentFound = this->otherParents != nullptr && this->otherParents->MapUntil([&](int index, RecyclerWeakReference<ArrayBufferParent>* item)
            {
                if (item->Get() == parent)
                {
                    foundIndex = index;
                    return true;
                }
                return false;

            });

            if (parentFound)
            {
                this->otherParents->RemoveAt(foundIndex);
            }
            else
            {
                AssertMsg(false, "We shouldn't be clearing a parent that hasn't been set.");
            }
        }
    }

    uint32 ArrayBuffer::GetByteLengthFromVar(ScriptContext* scriptContext, Var length)
    {
        Var firstArgument = length;
        if (TaggedInt::Is(firstArgument))
        {
            int32 byteCount = TaggedInt::ToInt32(firstArgument);
            if (byteCount < 0)
            {
                JavascriptError::ThrowRangeError(
                    scriptContext, JSERR_ArrayLengthConstructIncorrect);
            }
            return byteCount;
        }
        else
        {
            return JavascriptConversion::ToUInt32(firstArgument, scriptContext);
        }
    }

    Var ArrayBuffer::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && RecyclableObject::Is(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);

        if (!(callInfo.Flags & CallFlags_New) || (newTarget && JavascriptOperators::IsUndefinedObject(newTarget)))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, L"ArrayBuffer");
        }

        uint32 byteLength = 0;
        if (args.Info.Count > 1)
        {
            byteLength = GetByteLengthFromVar(scriptContext, args[1]);
        }

        RecyclableObject* newArr = scriptContext->GetLibrary()->CreateArrayBuffer(byteLength);
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {
            newArr = Js::JavascriptProxy::AutoProxyWrapper(newArr);
        }
#endif
        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), newArr, nullptr, scriptContext) :
            newArr;
    }

    // ArrayBuffer.prototype.byteLength as described in ES6 draft #20 section 24.1.4.1
    Var ArrayBuffer::EntryGetterByteLength(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !ArrayBuffer::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        ArrayBuffer* arrayBuffer = ArrayBuffer::FromVar(args[0]);
        if (arrayBuffer->IsDetached())
        {
            return JavascriptNumber::ToVar(0, scriptContext);
        }
        return JavascriptNumber::ToVar(arrayBuffer->GetByteLength(), scriptContext);
    }

    // ArrayBuffer.isView as described in ES6 draft #20 section 24.1.3.1
    Var ArrayBuffer::EntryIsView(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        JavascriptLibrary* library = function->GetScriptContext()->GetLibrary();

        Var arg = library->GetUndefined();

        if (args.Info.Count > 1)
        {
            arg = args[1];
        }

        // Only DataView or any TypedArray objects have [[ViewedArrayBuffer]] internal slots
        if (DataView::Is(arg) || TypedArrayBase::Is(arg))
        {
            return library->GetTrue();
        }

        return library->GetFalse();
    }

    // ArrayBuffer.transfer as described in Luke Wagner's proposal: https://gist.github.com/lukewagner/2735af7eea411e18cf20
    Var ArrayBuffer::EntryTransfer(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ScriptContext* scriptContext = function->GetScriptContext();

        PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);

        Assert(!(callInfo.Flags & CallFlags_New));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ArrayBufferTransfer);

        if (args.Info.Count < 2 || !ArrayBuffer::Is(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        ArrayBuffer* arrayBuffer = ArrayBuffer::FromVar(args[1]);

        if (arrayBuffer->IsDetached())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"ArrayBuffer.transfer");
        }

        uint32 newBufferLength = arrayBuffer->bufferLength;
        if (args.Info.Count >= 3)
        {
            newBufferLength = GetByteLengthFromVar(scriptContext, args[2]);
        }

        if (newBufferLength > MaxArrayBufferLength)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_Invalid);
        }

        return arrayBuffer->TransferInternal(newBufferLength);
    }

    // ArrayBuffer.prototype.slice as described in ES6 draft #19 section 24.1.4.3.
    Var ArrayBuffer::EntrySlice(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ScriptContext* scriptContext = function->GetScriptContext();

        PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Assert(!(callInfo.Flags & CallFlags_New));

        if (!ArrayBuffer::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        JavascriptLibrary* library = scriptContext->GetLibrary();
        ArrayBuffer* arrayBuffer = ArrayBuffer::FromVar(args[0]);

        if (arrayBuffer->IsDetached()) // 24.1.4.3: 5. If IsDetachedBuffer(O) is true, then throw a TypeError exception.
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"ArrayBuffer.prototype.slice");
        }

        int64 len = arrayBuffer->bufferLength;
        int64 start = 0, end = 0;
        int64 newLen;

        // If no start or end arguments, use the entire length
        if (args.Info.Count < 2)
        {
            newLen = len;
        }
        else
        {
            start = JavascriptArray::GetIndexFromVar(args[1], len, scriptContext);

            // If no end argument, use length as the end
            if (args.Info.Count < 3 || args[2] == library->GetUndefined())
            {
                end = len;
            }
            else
            {
                end = JavascriptArray::GetIndexFromVar(args[2], len, scriptContext);
            }

            newLen = end > start ? end - start : 0;
        }

        // We can't have allocated an ArrayBuffer with byteLength > MaxArrayBufferLength.
        // start and end are clamped to valid indices, so the new length also cannot exceed MaxArrayBufferLength.
        // Therefore, should be safe to cast down newLen to uint32.
        // TODO: If we ever support allocating ArrayBuffer with byteLength > MaxArrayBufferLength we may need to review this math.
        Assert(newLen < MaxArrayBufferLength);
        uint32 byteLength = static_cast<uint32>(newLen);

        ArrayBuffer* newBuffer = nullptr;

        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {
            Var constructorVar = JavascriptOperators::SpeciesConstructor(arrayBuffer, scriptContext->GetLibrary()->GetArrayBufferConstructor(), scriptContext);

            JavascriptFunction* constructor = JavascriptFunction::FromVar(constructorVar);

            Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(byteLength, scriptContext) };
            Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
            Js::Var newVar = JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext);

            if (!ArrayBuffer::Is(newVar)) // 24.1.4.3: 19.If new does not have an [[ArrayBufferData]] internal slot throw a TypeError exception.
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
            }

            newBuffer = ArrayBuffer::FromVar(newVar);

            if (newBuffer->IsDetached()) // 24.1.4.3: 21. If IsDetachedBuffer(new) is true, then throw a TypeError exception.
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"ArrayBuffer.prototype.slice");
            }

            if (newBuffer == arrayBuffer) // 24.1.4.3: 22. If SameValue(new, O) is true, then throw a TypeError exception.
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
            }

            if (newBuffer->bufferLength < byteLength) // 24.1.4.3: 23.If the value of new's [[ArrayBufferByteLength]] internal slot < newLen, then throw a TypeError exception.
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_ArgumentOutOfRange, L"ArrayBuffer.prototype.slice");
            }
        }
        else
        {
            newBuffer = library->CreateArrayBuffer(byteLength);
        }

        Assert(newBuffer);
        Assert(newBuffer->bufferLength >= byteLength);

        if (arrayBuffer->IsDetached()) // 24.1.4.3: 24. NOTE: Side-effects of the above steps may have detached O. 25. If IsDetachedBuffer(O) is true, then throw a TypeError exception.
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, L"ArrayBuffer.prototype.slice");
        }

        // Don't bother doing memcpy if we aren't copying any elements
        if (byteLength > 0)
        {
            AssertMsg(arrayBuffer->buffer != nullptr, "buffer must not be null when we copy from it");

            js_memcpy_s(newBuffer->buffer, byteLength, arrayBuffer->buffer + start, byteLength);
        }

        return newBuffer;
    }

    Var ArrayBuffer::EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);

        Assert(args.Info.Count > 0);

        return args[0];
    }

    ArrayBuffer* ArrayBuffer::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "var must be an ArrayBuffer");

        return static_cast<ArrayBuffer *>(RecyclableObject::FromVar(aValue));
    }

    bool  ArrayBuffer::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_ArrayBuffer;
    }

    template <class Allocator>
    ArrayBuffer::ArrayBuffer(uint32 length, DynamicType * type, Allocator allocator) :
        DynamicObject(type), mIsAsmJsBuffer(false), isBufferCleared(false),isDetached(false)
    {
        buffer = nullptr;
        bufferLength = 0;
        if (length > MaxArrayBufferLength)
        {
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_FunctionArgument_Invalid);
        }
        else if (length > 0)
        {
            Recycler* recycler = GetType()->GetLibrary()->GetRecycler();
            if (recycler->ReportExternalMemoryAllocation(length))
            {
                buffer = (BYTE*)allocator(length);
                if (buffer == nullptr)
                {
                    recycler->CollectNow<CollectOnTypedArrayAllocation>();
                    buffer = (BYTE*)allocator(length);
                    if (buffer == nullptr)
                    {
                        recycler->ReportExternalMemoryFailure(length);
                    }
                }
            }

            if (buffer == nullptr)
            {
                JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
            }

            bufferLength = length;
            ZeroMemory(buffer, bufferLength);
        }
    }

    ArrayBuffer::ArrayBuffer(byte* buffer, uint32 length, DynamicType * type) :
        buffer(buffer), bufferLength(length), DynamicObject(type), mIsAsmJsBuffer(false), isDetached(false)
    {
        if (length > MaxArrayBufferLength)
        {
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_FunctionArgument_Invalid);
        }
    }


    inline BOOL ArrayBuffer::IsBuiltinProperty(PropertyId propertyId)
    {
        // byteLength is only an instance property in pre-ES6
        if (propertyId == PropertyIds::byteLength
            && !GetScriptContext()->GetConfig()->IsES6TypedArrayExtensionsEnabled())
            return TRUE;

        return FALSE;
    }

    BOOL ArrayBuffer::HasProperty(PropertyId propertyId)
    {
        if (IsBuiltinProperty(propertyId))
        {
            return true;
        }

        return DynamicObject::HasProperty(propertyId);
    }

    BOOL ArrayBuffer::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {
        if (IsBuiltinProperty(propertyId))
        {
            return false;
        }
        return DynamicObject::DeleteProperty(propertyId, flags);

    }

    BOOL ArrayBuffer::GetSpecialPropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext)
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
    uint ArrayBuffer::GetSpecialPropertyCount() const
    {
        return this->GetScriptContext()->GetConfig()->IsES6TypedArrayExtensionsEnabled() ?
            0 : _countof(specialPropertyIds);
    }

    // Returns the list of special non-enumerable properties for the type.
    PropertyId const * ArrayBuffer::GetSpecialPropertyIds() const
    {
        return specialPropertyIds;
    }

    BOOL ArrayBuffer::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return ArrayBuffer::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL ArrayBuffer::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        BOOL result;
        if (GetPropertyBuiltIns(propertyId, value, &result))
        {
            return result;
        }

        return DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL ArrayBuffer::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, &result))
        {
            return result;
        }

        return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    bool ArrayBuffer::GetPropertyBuiltIns(PropertyId propertyId, Var* value, BOOL* result)
    {
        // byteLength is only an instance property in pre-ES6
        if (propertyId == PropertyIds::byteLength
            && !GetScriptContext()->GetConfig()->IsES6TypedArrayExtensionsEnabled())
        {
            *value = JavascriptNumber::ToVar(this->GetByteLength(), GetScriptContext());
            *result = true;
            return true;
        }

        return false;
    }

    BOOL ArrayBuffer::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        if (IsBuiltinProperty(propertyId))
        {
            return false;
        }
        else
        {
            return DynamicObject::SetProperty(propertyId, value, flags, info);
        }
    }

    BOOL ArrayBuffer::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && IsBuiltinProperty(propertyRecord->GetPropertyId()))
        {
            return false;
        }
        else
        {
            return DynamicObject::SetProperty(propertyNameString, value, flags, info);
        }
    }


    BOOL ArrayBuffer::IsEnumerable(PropertyId propertyId)
    {
        if (IsBuiltinProperty(propertyId))
        {
            return false;
        }
        return DynamicObject::IsEnumerable(propertyId);
    }

    BOOL ArrayBuffer::IsConfigurable(PropertyId propertyId)
    {
        if (IsBuiltinProperty(propertyId))
        {
            return false;
        }
        return DynamicObject::IsConfigurable(propertyId);
    }


    BOOL ArrayBuffer::InitProperty(Js::PropertyId propertyId, Js::Var value, PropertyOperationFlags flags, Js::PropertyValueInfo* info)
    {
        return SetProperty(propertyId, value, flags, info);
    }

    BOOL ArrayBuffer::IsWritable(PropertyId propertyId)
    {
        if (IsBuiltinProperty(propertyId))
        {
            return false;
        }
        return DynamicObject::IsWritable(propertyId);
    }


    BOOL ArrayBuffer::SetEnumerable(PropertyId propertyId, BOOL value)
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

        return __super::SetEnumerable(propertyId, value);
    }

    BOOL ArrayBuffer::SetWritable(PropertyId propertyId, BOOL value)
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

        return __super::SetWritable(propertyId, value);
    }

    BOOL ArrayBuffer::SetConfigurable(PropertyId propertyId, BOOL value)
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

        return __super::SetConfigurable(propertyId, value);
    }

    BOOL ArrayBuffer::SetAttributes(PropertyId propertyId, PropertyAttributes attributes)
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

        return __super::SetAttributes(propertyId, attributes);
    }

    BOOL ArrayBuffer::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        if (IsBuiltinProperty(propertyId))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                GetScriptContext()->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
        }

        return __super::SetAccessors(propertyId, getter, setter, flags);
    }



    BOOL ArrayBuffer::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
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

        return __super::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    BOOL ArrayBuffer::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"Object, (ArrayBuffer)");
        return TRUE;
    }

    BOOL ArrayBuffer::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"[object ArrayBuffer]");
        return TRUE;
    }


    JavascriptArrayBuffer::JavascriptArrayBuffer(uint32 length, DynamicType * type) :
        ArrayBuffer(length, type, (IsValidVirtualBufferLength(length)) ? AllocWrapper : malloc)
    {
    }
    JavascriptArrayBuffer::JavascriptArrayBuffer(byte* buffer, uint32 length, DynamicType * type) :
        ArrayBuffer(buffer, length, type)
    {
    }

    JavascriptArrayBuffer::JavascriptArrayBuffer(DynamicType * type): ArrayBuffer(0, type, malloc)
    {
    }

    JavascriptArrayBuffer* JavascriptArrayBuffer::Create(uint32 length, DynamicType * type)
    {
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        JavascriptArrayBuffer* result = RecyclerNewFinalized(recycler, JavascriptArrayBuffer, length, type);
        Assert(result);
        recycler->AddExternalMemoryUsage(length);
        return result;
    }

    JavascriptArrayBuffer* JavascriptArrayBuffer::Create(byte* buffer, uint32 length, DynamicType * type)
    {
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        JavascriptArrayBuffer* result = RecyclerNewFinalized(recycler, JavascriptArrayBuffer, buffer, length, type);
        Assert(result);
        recycler->AddExternalMemoryUsage(length);
        return result;
    }

    ArrayBufferDetachedStateBase* JavascriptArrayBuffer::CreateDetachedState(BYTE* buffer, uint32 bufferLength)
    {
#if _WIN64
        if (IsValidVirtualBufferLength(bufferLength))
        {
            return HeapNew(ArrayBufferDetachedState<FreeFn>, buffer, bufferLength, FreeMemAlloc, ArrayBufferAllocationType::MemAlloc);
        }
        else
        {
            return HeapNew(ArrayBufferDetachedState<FreeFn>, buffer, bufferLength, free, ArrayBufferAllocationType::Heap);
        }
#else
        return HeapNew(ArrayBufferDetachedState<FreeFn>, buffer, bufferLength, free, ArrayBufferAllocationType::Heap);
#endif
    }

    bool JavascriptArrayBuffer::IsValidAsmJsBufferLength(uint length, bool forceCheck)
    {
#if _WIN64
        /*
        1. length >= 2^16
        2. length is power of 2 or (length > 2^24 and length is multiple of 2^24)
        3. length is a multiple of 4K
        */
        return ((length >= 0x10000) &&
            (((length & (~length + 1)) == length) ||
            (length >= 0x1000000 && ((length & 0xFFFFFF) == 0))
            ) &&
            ((length % AutoSystemInfo::PageSize) == 0)
            );

#else
        if (forceCheck)
        {
            return ((length >= 0x10000) &&
                (((length & (~length + 1)) == length) ||
                (length >= 0x1000000 && ((length & 0xFFFFFF) == 0))
                ) &&
                ((length & 0x0FFF) == 0)
                );
        }
        return false;
#endif

    }

    bool JavascriptArrayBuffer::IsValidVirtualBufferLength(uint length)
    {

#if _WIN64
        /*
        1. length >= 2^16
        2. length is power of 2 or (length > 2^24 and length is multiple of 2^24)
        3. length is a multiple of 4K
        */
        return (!PHASE_OFF1(Js::TypedArrayVirtualPhase) &&
            (length >= 0x10000) &&
            (((length & (~length + 1)) == length) ||
            (length >= 0x1000000 &&
            ((length & 0xFFFFFF) == 0)
            )
            ) &&
            ((length % AutoSystemInfo::PageSize) == 0)
            );
#else
        return false;
#endif
    }

    void JavascriptArrayBuffer::Finalize(bool isShutdown)
    {
            // In debugger scenario, ScriptAuthor can create scriptContxt and delete scriptContext
            // explicitly. So for the builtin, while javascriptLibrary is still alive fine, the
            // matching scriptContext might have been deleted and the javascriptLibrary->scriptContext
            // field reset (but javascriptLibrary is still alive).
            // Use the recycler field off library instead of scriptcontext to avoid av.
            Recycler* recycler = GetType()->GetLibrary()->GetRecycler();
            recycler->ReportExternalMemoryFree(bufferLength);
    }

    void JavascriptArrayBuffer::Dispose(bool isShutdown)
    {

#if _WIN64
        //AsmJS Virtual Free
        //TOD - see if isBufferCleared need to be added for free too
        if (IsValidVirtualBufferLength(this->bufferLength) && !isBufferCleared)
        {
            LPVOID startBuffer = (LPVOID)((uint64)buffer);
            BOOL fSuccess = VirtualFree((LPVOID)startBuffer, 0, MEM_RELEASE);
            Assert(fSuccess);
            isBufferCleared = true;
        }
        else
        {
            free(buffer);
        }
#else
        free(buffer);
#endif
    }

    ArrayBuffer * JavascriptArrayBuffer::TransferInternal(uint32 newBufferLength)
    {
        ArrayBuffer* newArrayBuffer;
        Recycler* recycler = this->GetRecycler();

        // Report differential external memory allocation.
        // If current bufferLength == 0, new ArrayBuffer creation records the allocation
        // so no need to do it here.
        if (this->bufferLength > 0 && newBufferLength != this->bufferLength)
        {
            // Expanding buffer
            if (newBufferLength > this->bufferLength)
            {
                if (!recycler->ReportExternalMemoryAllocation(newBufferLength - this->bufferLength))
                {
                    JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                }
            }
            // Contracting buffer
            else
            {
                recycler->ReportExternalMemoryFree(this->bufferLength - newBufferLength);
            }
        }

        if (newBufferLength == 0 || this->bufferLength == 0)
        {
            newArrayBuffer = GetLibrary()->CreateArrayBuffer(newBufferLength);
        }
        else
        {
            BYTE * newBuffer = nullptr;
            if (IsValidVirtualBufferLength(this->bufferLength))
            {
                if (IsValidVirtualBufferLength(newBufferLength))
                {
                    // we are transferring between an optimized buffer using a length that can be optimized
                    if (newBufferLength < this->bufferLength)
                    {
#pragma prefast(suppress:6250, "Calling 'VirtualFree' without the MEM_RELEASE flag might free memory but not address descriptors (VADs).")
                        VirtualFree(this->buffer + newBufferLength, this->bufferLength - newBufferLength, MEM_DECOMMIT);
                    }
                    else if (newBufferLength > this->bufferLength)
                    {
                        LPVOID newMem = VirtualAlloc(this->buffer + this->bufferLength, newBufferLength - this->bufferLength, MEM_COMMIT, PAGE_READWRITE);
                        if (!newMem)
                        {
                            recycler->ReportExternalMemoryFailure(newBufferLength);
                            JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                        }
                    }
                    newBuffer = this->buffer;
                }
                else
                {
                    // we are transferring from an optimized buffer, but the new length isn't compatible, so start over and copy to new memory
                    newBuffer = (BYTE*)malloc(newBufferLength);
                    if (!newBuffer)
                    {
                        recycler->ReportExternalMemoryFailure(newBufferLength);
                        JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                    }
                    js_memcpy_s(newBuffer, newBufferLength, this->buffer, newBufferLength);
                }
            }
            else
            {
                if (IsValidVirtualBufferLength(newBufferLength))
                {
                    // we are transferring from an unoptimized buffer, but new length can be optimized, so move to that
                    newBuffer = (BYTE*)JavascriptArrayBuffer::AllocWrapper(newBufferLength);
                    js_memcpy_s(newBuffer, newBufferLength, this->buffer, newBufferLength);
                }
                else if (newBufferLength != this->bufferLength)
                {
                    // both sides will just be regular ArrayBuffer, so realloc
                    newBuffer = (BYTE*)realloc(this->buffer, newBufferLength);
                    if (!newBuffer)
                    {
                        recycler->ReportExternalMemoryFailure(newBufferLength);
                        JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                    }
                }
                else
                {
                    newBuffer = this->buffer;
                }
            }
            newArrayBuffer = GetLibrary()->CreateArrayBuffer(newBuffer, newBufferLength);

        }
        AutoDiscardPTR<Js::ArrayBufferDetachedStateBase> state(DetachAndGetState());
        state->MarkAsClaimed();

        return newArrayBuffer;
    }

    ProjectionArrayBuffer::ProjectionArrayBuffer(uint32 length, DynamicType * type) :
        ArrayBuffer(length, type, CoTaskMemAlloc)
    {
    }

    ProjectionArrayBuffer::ProjectionArrayBuffer(byte* buffer, uint32 length, DynamicType * type) :
        ArrayBuffer(buffer, length, type)
    {
    }

    ProjectionArrayBuffer* ProjectionArrayBuffer::Create(uint32 length, DynamicType * type)
    {
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        recycler->AddExternalMemoryUsage(length);
        return RecyclerNewFinalized(recycler, ProjectionArrayBuffer, length, type);
    }

    ProjectionArrayBuffer* ProjectionArrayBuffer::Create(byte* buffer, uint32 length, DynamicType * type)
    {
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        // This is user passed [in] buffer, user should AddExternalMemoryUsage before calling jscript, but
        // I don't see we ask everyone to do this. Let's add the memory pressure here as well.
        recycler->AddExternalMemoryUsage(length);
        return RecyclerNewFinalized(recycler, ProjectionArrayBuffer, buffer, length, type);
    }

    void ProjectionArrayBuffer::Dispose(bool isShutdown)
    {
        CoTaskMemFree(buffer);
    }

    ArrayBuffer * ProjectionArrayBuffer::TransferInternal(uint32 newBufferLength)
    {
        ArrayBuffer* newArrayBuffer;
        if (newBufferLength == 0 || this->bufferLength == 0)
        {
            newArrayBuffer = GetLibrary()->CreateProjectionArraybuffer(newBufferLength);
        }
        else
        {
            BYTE * newBuffer = (BYTE*)CoTaskMemRealloc(this->buffer, newBufferLength);
            if (!newBuffer)
            {
                JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
            }
            newArrayBuffer = GetLibrary()->CreateProjectionArraybuffer(newBuffer, newBufferLength);
        }

        AutoDiscardPTR<Js::ArrayBufferDetachedStateBase> state(DetachAndGetState());
        state->MarkAsClaimed();

        return newArrayBuffer;
    }

    ExternalArrayBuffer::ExternalArrayBuffer(byte *buffer, uint32 length, DynamicType *type)
        : ArrayBuffer(buffer, length, type)
    {
    }
}

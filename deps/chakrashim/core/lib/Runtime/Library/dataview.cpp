//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//  Implements typed array.
//----------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{

    Var DataView::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && RecyclableObject::Is(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);
        uint32 byteLength = 0;
        uint32 mappedLength;
        int32 offset = 0;
        double numberOffset = 0;
        ArrayBuffer* arrayBuffer = nullptr;
        DataView* dataView;

        //1.    If NewTarget is undefined, throw a TypeError exception.
        if (!(callInfo.Flags & CallFlags_New) || (newTarget && JavascriptOperators::IsUndefinedObject(newTarget)))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, L"DataView");
        }

        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument, L"buffer");
        }

        // Currently the only reason we check for an external object is projection related, so it remains under conditional compilation.
        RecyclableObject* jsArraySource = NULL;
        if (JavascriptOperators::IsObject(args[1]) && JavascriptConversion::ToObject(args[1], scriptContext, &jsArraySource))
        {
            HRESULT hr = scriptContext->GetHostScriptContext()->ArrayBufferFromExternalObject(jsArraySource, &arrayBuffer);
            switch (hr)
            {
            case S_OK:
            case S_FALSE:
                // Both of these cases will be handled by the arrayBuffer null check.
                break;

            default:
                // Any FAILURE HRESULT or unexpected HRESULT.
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_InvalidArugment, L"buffer");
                break;
            }
        }

        //2.    If Type(buffer) is not Object, throw a TypeError exception.
        //3.    If buffer does not have an [[ArrayBufferData]] internal slot, throw a TypeError exception.
        if (arrayBuffer == nullptr)
        {
            if (ArrayBuffer::Is(args[1]))
            {
                arrayBuffer = ArrayBuffer::FromVar(args[1]);
            }
            else
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument, L"buffer");
            }
        }

        //4.    Let numberOffset be ToNumber(byteOffset).
        //5.    Let offset be ToInteger(numberOffset).
        //6.    ReturnIfAbrupt(offset).
        //7.    If numberOffset <> offset or offset < 0, throw a RangeError exception.
        if (args.Info.Count > 2)
        {
            Var secondArgument = args[2];
            numberOffset = JavascriptConversion::ToNumber(secondArgument, scriptContext);
            offset = JavascriptConversion::ToInt32(numberOffset);

            if (offset < 0 ||
                numberOffset != offset)
            {
                JavascriptError::ThrowRangeError(
                    scriptContext, JSERR_DataView_InvalidArugment, L"byteOffset");
            }
        }

        //8.    If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
        if (arrayBuffer->IsDetached())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }

        //9.    Let bufferByteLength be the value of buffer's[[ArrayBufferByteLength]] internal slot.
        //10.   If offset > bufferByteLength, throw a RangeError exception.

        byteLength = arrayBuffer->GetByteLength();
        if ((uint32)offset > byteLength)
        {
            JavascriptError::ThrowRangeError(
                scriptContext, JSERR_DataView_InvalidArugment, L"byteOffset");
        }

        //11.   If byteLength is undefined, then
        //      a.  Let viewByteLength be bufferByteLength - offset.
        //12.   Else,
        //      a.  Let viewByteLength be ToLength(byteLength).
        //      b.  ReturnIfAbrupt(viewLength).
        //      c.  If offset + viewByteLength > bufferByteLength, throw a RangeError exception.
        if (args.Info.Count > 3 && !JavascriptOperators::IsUndefinedObject(args[3]))
            {
                Var thirdArgument = args[3];
                mappedLength = (uint32)JavascriptConversion::ToLength(thirdArgument, scriptContext);
                uint32 viewRange = mappedLength + offset;

                if (viewRange > byteLength || viewRange < mappedLength) // overflow indicates out-of-range
                {
                    JavascriptError::ThrowRangeError(
                        scriptContext, JSERR_DataView_InvalidArugment, L"byteLength");
                }
            }
        else
        {
            mappedLength = byteLength - offset;
        }

        //13.   Let O be OrdinaryCreateFromConstructor(NewTarget, "%DataViewPrototype%", [[DataView]], [[ViewedArrayBuffer]], [[ByteLength]], [[ByteOffset]]).
        //14.   ReturnIfAbrupt(O).
        //15.   Set O's[[DataView]] internal slot to true.
        //16.   Set O's[[ViewedArrayBuffer]] internal slot to buffer.
        //17.   Set O's[[ByteLength]] internal slot to viewByteLength.
        //18.   Set O's[[ByteOffset]] internal slot to offset.
        //19.   Return O.
        dataView = scriptContext->GetLibrary()->CreateDataView(arrayBuffer, offset, mappedLength);
        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), dataView, nullptr, scriptContext) :
            dataView;
    }

    DataView::DataView(ArrayBuffer* arrayBuffer, uint32 byteoffset, uint32 mappedLength, DynamicType* type)
        : ArrayBufferParent(type, mappedLength, arrayBuffer),
          byteOffset(byteoffset)
    {
        buffer = arrayBuffer->GetBuffer() + byteoffset;
    }

    BOOL DataView::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_DataView;
    }

    Var DataView::EntryGetInt8(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument, L"offset");
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValue<int8>(offset, L"DataView.prototype.GetInt8", FALSE);
    }

    Var DataView::EntryGetUint8(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  L"offset");
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValue<uint8>(offset, L"DataView.prototype.GetUint8", FALSE);
    }

    Var DataView::EntryGetInt16(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument, L"offset");
        }
        if (args.Info.Count > 2)
        {
            isLittleEndian = JavascriptConversion::ToBoolean(args[2], scriptContext);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValue<int16>(offset, L"DataView.prototype.GetInt16", isLittleEndian);
    }

    Var DataView::EntryGetUint16(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  L"offset");
        }
        if (args.Info.Count > 2)
        {
            isLittleEndian = JavascriptConversion::ToBoolean(args[2], scriptContext);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValue<uint16>(offset, L"DataView.prototype.GetUint16", isLittleEndian);
    }

    Var DataView::EntryGetUint32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  L"offset");
        }
        if (args.Info.Count > 2)
        {
            isLittleEndian = JavascriptConversion::ToBoolean(args[2], scriptContext);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValue<uint32>(offset, L"DataView.prototype.GetUint32", isLittleEndian);
    }

    Var DataView::EntryGetInt32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument, L"offset");
        }
        if (args.Info.Count > 2)
        {
            isLittleEndian = JavascriptConversion::ToBoolean(args[2], scriptContext);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValue<int32>(offset, L"DataView.prototype.GetInt32", isLittleEndian);
    }

    Var DataView::EntryGetFloat32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  L"offset");
        }
        if (args.Info.Count > 2)
        {
            isLittleEndian = JavascriptConversion::ToBoolean(args[2], scriptContext);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValueWithCheck<float>(offset, L"DataView.prototype.GetFloat32", isLittleEndian);
    }

    Var DataView::EntryGetFloat64(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  L"offset");
        }
        if (args.Info.Count > 2)
        {
            isLittleEndian = JavascriptConversion::ToBoolean(args[2], scriptContext);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValueWithCheck<double>(offset, L"DataView.prototype.GetFloat64", isLittleEndian);
    }

    Var DataView::EntrySetInt8(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  L"offset or value");
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        int8 value = JavascriptConversion::ToInt8(args[2], scriptContext);
        dataView->SetValue<int8>(offset, value, L"DataView.prototype.SetInt8");
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetUint8(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  L"offset or value");
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        uint8 value = JavascriptConversion::ToUInt8(args[2], scriptContext);
        dataView->SetValue<uint8>(offset, value, L"DataView.prototype.SetUint8");
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetInt16(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  L"offset or value");
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        int16 value = JavascriptConversion::ToInt16(args[2], scriptContext);
        if (args.Info.Count > 3)
        {
            isLittleEndian = JavascriptConversion::ToBoolean(args[3], scriptContext);
        }
        dataView->SetValue<int16>(offset, value, L"DataView.prototype.SetInt16", isLittleEndian);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetUint16(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument, L"offset or value");
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        uint16 value = JavascriptConversion::ToUInt16(args[2], scriptContext);
        if (args.Info.Count > 3)
        {
            isLittleEndian = JavascriptConversion::ToBoolean(args[3], scriptContext);
        }
        dataView->SetValue<uint16>(offset, value, L"DataView.prototype.SetUint16", isLittleEndian);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetInt32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  L"offset or value");
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        int32 value = JavascriptConversion::ToInt32(args[2], scriptContext);
        if (args.Info.Count > 3)
        {
            isLittleEndian = JavascriptConversion::ToBoolean(args[3], scriptContext);
        }
        dataView->SetValue<int32>(offset, value, L"DataView.prototype.SetInt32", isLittleEndian);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetUint32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  L"offset or value");
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        uint32 value = JavascriptConversion::ToUInt32(args[2], scriptContext);
        if (args.Info.Count > 3)
        {
            isLittleEndian = JavascriptConversion::ToBoolean(args[3], scriptContext);
        }
        dataView->SetValue<uint32>(offset, value, L"DataView.prototype.SetUint32", isLittleEndian);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetFloat32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView,  L"offset or value");
        }
        if (args.Info.Count < 3)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument);
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        float value = JavascriptConversion::ToFloat(args[2], scriptContext);
        if (args.Info.Count > 3)
        {
            isLittleEndian = JavascriptConversion::ToBoolean(args[3], scriptContext);
        }
        dataView->SetValue<float>(offset, value, L"DataView.prototype.SetFloat32", isLittleEndian);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetFloat64(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  L"offset or value");
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        double value = JavascriptConversion::ToNumber(args[2], scriptContext);
        if (args.Info.Count > 3)
        {
            isLittleEndian = JavascriptConversion::ToBoolean(args[3], scriptContext);
        }
        dataView->SetValue<double>(offset, value, L"DataView.prototype.SetFloat64", isLittleEndian);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntryGetterBuffer(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        ArrayBuffer* arrayBuffer = dataView->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        return arrayBuffer;
    }

    Var DataView::EntryGetterByteLength(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        ArrayBuffer* arrayBuffer = dataView->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }
        else if (arrayBuffer->IsDetached())
        {
            return TaggedInt::ToVarUnchecked(0);
        }

        return JavascriptNumber::ToVar(dataView->GetLength(), scriptContext);
    }

    Var DataView::EntryGetterByteOffset(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        ArrayBuffer* arrayBuffer = dataView->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }
        else if (arrayBuffer->IsDetached())
        {
            return TaggedInt::ToVarUnchecked(0);
        }

        return JavascriptNumber::ToVar(dataView->GetByteOffset(), scriptContext);
    }

    BOOL DataView::SetItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes)
    {
        AssertMsg(false, "We don't need a DataView to serve as object's internal array");
        return FALSE;
    }

#ifdef _M_ARM
        // Provide template specialization (only) for memory access at unaligned float/double address which causes data alignment exception otherwise.
        template<>
        Var DataView::GetValueWithCheck<float>(uint32 byteOffset, wchar_t *funcName, BOOL isLittleEndian = FALSE)
        {
            return this->GetValueWithCheck<float, float UNALIGNED*>(byteOffset, isLittleEndian, funcName);
        }

        template<>
        Var DataView::GetValueWithCheck<double>(uint32 byteOffset, wchar_t *funcName, BOOL isLittleEndian = FALSE)
        {
            return this->GetValueWithCheck<double, double UNALIGNED*>(byteOffset, isLittleEndian, funcName);
        }

        template<>
        void DataView::SetValue<float>(uint32 byteOffset, float value, wchar_t *funcName, BOOL isLittleEndian = FALSE)
        {
            this->SetValue<float, float UNALIGNED*>(byteOffset, value, isLittleEndian, funcName);
        }

        template<>
        void DataView::SetValue<double>(uint32 byteOffset, double value, wchar_t *funcName, BOOL isLittleEndian = FALSE)
        {
            this->SetValue<double, double UNALIGNED*>(byteOffset, value, isLittleEndian, funcName);
        }
#endif

}

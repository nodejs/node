//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class DataView : public ArrayBufferParent
    {
        friend ArrayBuffer;
    protected:
        DEFINE_VTABLE_CTOR(DataView, ArrayBufferParent);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(DataView);
    public:
        class EntryInfo
        {
        public:
            static FunctionInfo NewInstance;
            static FunctionInfo GetInt8;
            static FunctionInfo GetUint8;
            static FunctionInfo GetInt16;
            static FunctionInfo GetUint16;
            static FunctionInfo GetInt32;
            static FunctionInfo GetUint32;
            static FunctionInfo GetFloat32;
            static FunctionInfo GetFloat64;
            static FunctionInfo SetInt8;
            static FunctionInfo SetUint8;
            static FunctionInfo SetInt16;
            static FunctionInfo SetUint16;
            static FunctionInfo SetInt32;
            static FunctionInfo SetUint32;
            static FunctionInfo SetFloat32;
            static FunctionInfo SetFloat64;

            static FunctionInfo GetterBuffer;
            static FunctionInfo GetterByteLength;
            static FunctionInfo GetterByteOffset;
        };

        DataView(ArrayBuffer* arrayBuffer, uint32 byteOffset, uint32 mappedLength, DynamicType* type);

        static BOOL Is(Var aValue);

        static __inline DataView* FromVar(Var aValue)
        {
            Assert(DataView::Is(aValue));
            return static_cast<DataView*>(aValue);
        }

        uint32 GetByteOffset() const { return byteOffset; }

        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetInt8(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetUint8(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetInt16(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetUint16(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetInt32(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetUint32(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetFloat32(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetFloat64(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntrySetInt8(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetUint8(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetInt16(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetUint16(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetInt32(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetUint32(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetFloat32(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetFloat64(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryGetterBuffer(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetterByteLength(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetterByteOffset(RecyclableObject* function, CallInfo callInfo, ...);

        // objectArray support
        virtual BOOL SetItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes) override;

    private:
        template<typename TypeName>
        void SwapRoutine(TypeName* input, TypeName* dest);

        template<> void SwapRoutine(int8* input, int8* dest) {*dest =  *input; }
        template<> void SwapRoutine(uint8* input, uint8* dest) {*dest =  *input; }
        template<> void SwapRoutine(int16* input, int16* dest) {*dest =  RtlUshortByteSwap(*input); }
        template<> void SwapRoutine(uint16* input, uint16* dest) {*dest =  RtlUshortByteSwap(*input);}
        template<> void SwapRoutine(int32* input, int32* dest) {*dest =  RtlUlongByteSwap(*input);}
        template<> void SwapRoutine(uint32* input, uint32* dest) {*dest =  RtlUlongByteSwap(*input);}
        // we don't want type conversion here, we just want to swap the bytes.
        template<> void SwapRoutine(float* input, float* dest) { *((ulong*)dest) = RtlUlongByteSwap(*((ulong*)input)); }
        template<> void SwapRoutine(double* input, double* dest) {*((uint64*)dest) = RtlUlonglongByteSwap(*((uint64*)input)); }

        template<typename TypeName>
        Var GetValue(uint32 byteOffset, wchar_t* funcName, BOOL isLittleEndian = FALSE)
        {
            ScriptContext* scriptContext = GetScriptContext();
            if (this->GetArrayBuffer()->IsDetached())
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, funcName);
            }
            if ((byteOffset + sizeof(TypeName) <= GetLength()) && (byteOffset <= GetLength()))
            {
                TypeName item;
                TypeName* typedBuffer = (TypeName*)(buffer + byteOffset);
                if (!isLittleEndian)
                {
                    SwapRoutine<TypeName>(typedBuffer, &item);
                }
                else
                {
                    item = *typedBuffer;
                }
                return JavascriptNumber::ToVar(item, GetScriptContext());
            }
            else
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_InvalidOffset);
            }
        }

        template<typename TypeName>
        inline Var GetValueWithCheck(uint32 byteOffset, wchar_t* funcName, BOOL isLittleEndian = FALSE)
        {
            return GetValueWithCheck<TypeName, TypeName*>(byteOffset, isLittleEndian, funcName);
        }

        template<typename TypeName, typename PointerAccessTypeName>
        Var GetValueWithCheck(uint32 byteOffset, BOOL isLittleEndian, wchar_t* funcName)
        {
            ScriptContext* scriptContext = GetScriptContext();
            if (this->GetArrayBuffer()->IsDetached())
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, funcName);
            }
            if ((byteOffset + sizeof(TypeName) <= GetLength()) && (byteOffset <= GetLength()))
            {
                TypeName item;
                TypeName *typedBuffer = (TypeName*)(buffer + byteOffset);
                if (!isLittleEndian)
                {
                    SwapRoutine<TypeName>(typedBuffer, &item);
                }
                else
                {
                    item = *static_cast<PointerAccessTypeName>(typedBuffer);
                }
                return JavascriptNumber::ToVarWithCheck(item, GetScriptContext());
            }
            else
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_InvalidOffset);
            }
        }

        template<typename TypeName>
        inline void SetValue(uint32 byteOffset, TypeName value, wchar_t *funcName, BOOL isLittleEndian = FALSE)
        {
            SetValue<TypeName, TypeName*>(byteOffset, value, isLittleEndian, funcName);
        }

        template<typename TypeName, typename PointerAccessTypeName>
        void SetValue(uint32 byteOffset, TypeName value, BOOL isLittleEndian, wchar_t *funcName)
        {
            ScriptContext* scriptContext = GetScriptContext();
            if (this->GetArrayBuffer()->IsDetached())
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, funcName);
            }
            if ((byteOffset + sizeof(TypeName) <= GetLength()) && (byteOffset <= GetLength()))
            {
                TypeName* typedBuffer = (TypeName*)(buffer + byteOffset);
                if (!isLittleEndian)
                {
                    SwapRoutine<TypeName>(&value, typedBuffer);
                }
                else
                {
                    *static_cast<PointerAccessTypeName>(typedBuffer) = value;
                }
            }
            else
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_InvalidOffset);
            }
        }

#ifdef _M_ARM
        // For ARM, memory access for float/double address causes data alignment exception if the address is not aligned.
        // Provide template specilization (only) for these scenarios.
        template<> Var GetValueWithCheck<float>(uint32 byteOffset, wchar_t *funcName, BOOL isLittleEndian /* = FALSE */);
        template<> Var GetValueWithCheck<double>(uint32 byteOffset, wchar_t *funcName, BOOL isLittleEndian /* = FALSE */);
        template<> void SetValue<float>(uint32 byteOffset, float value, wchar_t *funcName, BOOL isLittleEndian /* = FALSE */);
        template<> void SetValue<double>(uint32 byteOffset, double value, wchar_t *funcName, BOOL isLittleEndian /* = FALSE */);
#endif

        uint32 byteOffset;
        BYTE* buffer;   // beginning of buffer

    };
}

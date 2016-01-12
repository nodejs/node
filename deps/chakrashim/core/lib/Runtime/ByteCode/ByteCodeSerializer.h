//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// Turn this on to enable magic constants in byte code (useful for debugging)
//#define BYTE_CODE_MAGIC_CONSTANTS

#include "ByteCode\ByteCodeSerializeFlags.h"

namespace Js
{
    // Some things are obscured by xor. This helps catch cases in which, for example, indirect property ids
    // are mistakenly mixed with actual property IDs.
#if DBG & VALIDATE_SERIALIZED_BYTECODE
    #define SERIALIZER_OBSCURE_PROPERTY_ID 0xdef00000
    #define SERIALIZER_OBSCURE_NONBUILTIN_PROPERTY_ID 0xdeb00000
    #define SERIALIZER_OBSCURE_LITERAL_OBJECT_ID 0xded00000
#else
    #define SERIALIZER_OBSCURE_PROPERTY_ID 0x00000000
    #define SERIALIZER_OBSCURE_NONBUILTIN_PROPERTY_ID 0x00000000
    #define SERIALIZER_OBSCURE_LITERAL_OBJECT_ID 0x00000000
#endif

    class ByteCodeBufferReader;

    enum SerializedAuxiliaryKind : byte
    {
        sakVarArrayIntCount = 1,
        sakVarArrayVarCount = 2,
        sakPropertyIdArray = 3,
        sakFuncInfoArray = 4,
        sakIntArray = 5,
        sakFloatArray = 6,

        sakPropertyIdArrayForCachedScope = 7,       // serialization only type, indiciate extra slots
    };

// Tightly pack serialized structures
#pragma pack(push, 1)

    // Describes the kind of auxiliary
    struct SerializedAuxiliary
    {
#ifdef BYTE_CODE_MAGIC_CONSTANTS
        int auxMagic; // magicStartOfAux
#endif
        uint offset;
        SerializedAuxiliaryKind kind;
        SerializedAuxiliary(uint offset, SerializedAuxiliaryKind kind);
    };

    // The in-memory layout of the serialized analog of VarArray
    struct SerializedVarArray : SerializedAuxiliary
    {
#ifdef BYTE_CODE_MAGIC_CONSTANTS
        int magic; // magicStartOfAuxVarArray
#endif
        int varCount;
        SerializedVarArray(uint offset, bool isVarCount, int varCount);
    };

    struct SerializedIntArray : SerializedAuxiliary
    {
#ifdef BYTE_CODE_MAGIC_CONSTANTS
        int magic; // magicStartOfAuxIntArray
#endif
        int intCount;
        SerializedIntArray(uint offset, int intCount);
    };

    struct SerializedFloatArray : SerializedAuxiliary
    {
#ifdef BYTE_CODE_MAGIC_CONSTANTS
        int magic; // magicStartOfAuxFltArray
#endif
        int floatCount;
        SerializedFloatArray(uint offset, int floatCount);
    };

    // The in-memory layout of the serialized analog of PropertyIdArray
    struct SerializedPropertyIdArray : SerializedAuxiliary
    {
#ifdef BYTE_CODE_MAGIC_CONSTANTS
        int magic; // magicStartOfAuxPropIdArray
#endif
        int propertyCount;
        int extraSlots;
        bool hadDuplicates;
        bool has__proto__;
        SerializedPropertyIdArray(uint offset, int propertyCount, int extraSlots, bool hadDuplicates, bool has__proto__);
    };

    // The in-memory layout of the serialized analog of FuncInfoArray
    struct SerializedFuncInfoArray : SerializedAuxiliary
    {
#ifdef BYTE_CODE_MAGIC_CONSTANTS
        int magic; // magicStartOfAuxFuncInfoArray
#endif
        int count;
        SerializedFuncInfoArray(uint offset, int count);
    };

#pragma pack(pop)

    // Holds information about the deserialized bytecode cache. Contains fast inline functions
    // for the lookup hit case. The slower deserialization of VarArray, etc are in the .cpp.
    class ByteCodeCache
    {
        ByteCodeBufferReader * reader;
        const byte * raw;
        PropertyId * propertyIds;
        int propertyCount;
        int builtInPropertyCount;
    public:
        ByteCodeCache(ScriptContext * scriptContext, ByteCodeBufferReader * reader, int builtInPropertyCount);
        void PopulateLookupPropertyId(ScriptContext * scriptContext, int realArrayOffset);

        ByteCodeBufferReader* GetReader()
        {
            return reader;
        }

        // Convert a serialized propertyID into a real one.
        inline PropertyId LookupPropertyId(PropertyId obscuredIdInCache) const
        {
            auto unobscured = obscuredIdInCache ^ SERIALIZER_OBSCURE_PROPERTY_ID;
            if (unobscured < builtInPropertyCount || unobscured==/*nil*/0xffffffff)
            {
                return unobscured; // This is a built in property id
            }
            auto realOffset = unobscured - builtInPropertyCount;
            Assert(realOffset<propertyCount);
            Assert(propertyIds[realOffset]!=-1);
            return propertyIds[realOffset];
        }

        // Convert a serialized propertyID into a real one.
        inline PropertyId LookupNonBuiltinPropertyId(PropertyId obscuredIdInCache) const
        {
            auto realOffset = obscuredIdInCache ^ SERIALIZER_OBSCURE_NONBUILTIN_PROPERTY_ID;
            Assert(realOffset<propertyCount);
            Assert(propertyIds[realOffset]!=-1);
            return propertyIds[realOffset];
        }

        // Get the raw byte code buffer.
        inline const byte * GetBuffer() const
        {
            return raw;
        }
    };

    // Methods for serializing and deserializing function bodies.
    struct ByteCodeSerializer
    {
        // Serialize a function body.
        static HRESULT SerializeToBuffer(ScriptContext * scriptContext, ArenaAllocator * alloc, DWORD sourceCodeLength, LPCUTF8 utf8Source, DWORD dwFunctionTableLength, BYTE * functionTable, FunctionBody * function, SRCINFO const* srcInfo, bool allocateBuffer, byte ** buffer, DWORD * bufferBytes, DWORD dwFlags = 0);

        // Deserialize a function body. The content of utf8Source must be the same as was originally passed to SerializeToBuffer
        static HRESULT DeserializeFromBuffer(ScriptContext * scriptContext, ulong scriptFlags, LPCUTF8 utf8Source, SRCINFO const * srcInfo, byte * buffer, NativeModule *nativeModule, FunctionBody** function, uint sourceIndex = Js::Constants::InvalidSourceIndex);
        static HRESULT DeserializeFromBuffer(ScriptContext * scriptContext, ulong scriptFlags, ISourceHolder* sourceHolder, SRCINFO const * srcInfo, byte * buffer, NativeModule *nativeModule, FunctionBody** function, uint sourceIndex = Js::Constants::InvalidSourceIndex);

        static FunctionBody* DeserializeFunction(ScriptContext* scriptContext, DeferDeserializeFunctionInfo* deferredFunction);

        // This lib doesn't directly depend on the generated interfaces. Ensure the same codes with a C_ASSERT
        static const HRESULT CantGenerate = 0x80020201L;
        static const HRESULT InvalidByteCode = 0x80020202L;

        static void ReadSourceInfo(const DeferDeserializeFunctionInfo* deferredFunction, int& lineNumber, int& columnNumber, bool& m_isEval, bool& m_isDynamicFunction);

    private:
        static HRESULT DeserializeFromBufferInternal(ScriptContext * scriptContext, ulong scriptFlags, LPCUTF8 utf8Source, ISourceHolder* sourceHolder, SRCINFO const * srcInfo, byte * buffer, NativeModule *nativeModule, FunctionBody** function, uint sourceIndex = Js::Constants::InvalidSourceIndex);
    };
}

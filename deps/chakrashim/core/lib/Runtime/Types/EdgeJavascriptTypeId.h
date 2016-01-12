//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
enum TypeId
{
    TypeIds_Undefined = 0,
    TypeIds_Null = 1,

    TypeIds_UndefinedOrNull =  TypeIds_Null,

    TypeIds_Boolean = 2,

    // backend typeof() == "number" is true for typeIds
    // between TypeIds_FirstNumberType <= typeId <= TypeIds_LastNumberType
    TypeIds_Integer = 3,
    TypeIds_FirstNumberType = TypeIds_Integer,
    TypeIds_Number = 4,
    TypeIds_Int64Number = 5,
    TypeIds_UInt64Number = 6,
    TypeIds_LastNumberType = TypeIds_UInt64Number,

    TypeIds_String = 7,
    TypeIds_Symbol = 8,

    TypeIds_LastToPrimitiveType = TypeIds_Symbol,

    TypeIds_Enumerator = 9,
    TypeIds_VariantDate = 10,

    // SIMD types
    TypeIds_SIMDFloat32x4 = 11,
    TypeIds_SIMDFloat64x2 = 12,
    TypeIds_SIMDInt32x4 = 13,
    TypeIds_SIMDInt8x16 = 14,

    TypeIds_LastJavascriptPrimitiveType = TypeIds_SIMDInt8x16,

    TypeIds_HostDispatch = 15,
    TypeIds_WithScopeObject = 16,
    TypeIds_UndeclBlockVar = 17,

    TypeIds_LastStaticType = TypeIds_UndeclBlockVar,

    TypeIds_Proxy = 18,
    TypeIds_Function = 19,

    //
    // The backend expects only objects whose typeof() === "object" to have a
    // TypeId >= TypeIds_Object. Only 'null' is a special case because it
    // has a static type.
    //
    TypeIds_Object = 20,
    TypeIds_Array = 21,
    TypeIds_ArrayFirst = TypeIds_Array,
    TypeIds_NativeIntArray = 22,
#if ENABLE_COPYONACCESS_ARRAY
    TypeIds_CopyOnAccessNativeIntArray = 23,
#endif
    TypeIds_NativeFloatArray = 24,
    TypeIds_ArrayLast = TypeIds_NativeFloatArray,
    TypeIds_Date = 25,
    TypeIds_RegEx = 26,
    TypeIds_Error = 27,
    TypeIds_BooleanObject = 28,
    TypeIds_NumberObject = 29,
    TypeIds_StringObject = 30,
    TypeIds_Arguments = 31,
    TypeIds_ES5Array = 32,
    TypeIds_ArrayBuffer = 33,
    TypeIds_Int8Array = 34,
    TypeIds_TypedArrayMin = TypeIds_Int8Array,
    TypeIds_TypedArraySCAMin = TypeIds_Int8Array, // Min SCA supported TypedArray TypeId
    TypeIds_Uint8Array = 35,
    TypeIds_Uint8ClampedArray = 36,
    TypeIds_Int16Array = 37,
    TypeIds_Uint16Array = 38,
    TypeIds_Int32Array = 39,
    TypeIds_Uint32Array = 40,
    TypeIds_Float32Array = 41,
    TypeIds_Float64Array = 42,
    TypeIds_TypedArraySCAMax = TypeIds_Float64Array, // Max SCA supported TypedArray TypeId
    TypeIds_Int64Array = 43,
    TypeIds_Uint64Array = 44,
    TypeIds_CharArray = 45,
    TypeIds_BoolArray = 46,
    TypeIds_TypedArrayMax = TypeIds_BoolArray,
    TypeIds_EngineInterfaceObject = 47,
    TypeIds_DataView = 48,
    TypeIds_WinRTDate = 49,
    TypeIds_Map = 50,
    TypeIds_Set = 51,
    TypeIds_WeakMap = 52,
    TypeIds_WeakSet = 53,
    TypeIds_SymbolObject = 54,
    TypeIds_ArrayIterator = 55,
    TypeIds_MapIterator = 56,
    TypeIds_SetIterator = 57,
    TypeIds_StringIterator = 58,
    TypeIds_JavascriptEnumeratorIterator = 59,
    TypeIds_Generator = 60,
    TypeIds_Promise = 61,

    TypeIds_LastBuiltinDynamicObject = TypeIds_Promise,
    TypeIds_GlobalObject = 62,
    TypeIds_ModuleRoot = 63,
    TypeIds_LastTrueJavascriptObjectType = TypeIds_ModuleRoot,

    TypeIds_HostObject = 64,
    TypeIds_ActivationObject = 65,
    TypeIds_SpreadArgument = 66,

    TypeIds_Limit //add a new TypeId before TypeIds_Limit or before TypeIds_LastTrueJavascriptObjectType
};



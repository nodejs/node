//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Value type bits

#ifdef VALUE_TYPE_BIT

VALUE_TYPE_BIT(Likely,                  static_cast<ValueType::TSize>(1 << 0    ))
VALUE_TYPE_BIT(Undefined,               static_cast<ValueType::TSize>(1 << 1    ))
VALUE_TYPE_BIT(Null,                    static_cast<ValueType::TSize>(1 << 2    ))
VALUE_TYPE_BIT(CanBeTaggedValue,        static_cast<ValueType::TSize>(1 << 3    ))
VALUE_TYPE_BIT(Object,                  static_cast<ValueType::TSize>(1 << 4    ))

#if !defined(VALUE_TYPE_OBJECT_BIT_INDEX)
#define VALUE_TYPE_OBJECT_BIT_INDEX static_cast<ValueType::TSize>(4)
#endif

#if !defined(VALUE_TYPE_COMMON_BIT_COUNT)
#define VALUE_TYPE_COMMON_BIT_COUNT static_cast<ValueType::TSize>(5)
#endif

// The following bits only apply when the Object bit is not set
VALUE_TYPE_BIT(Int,                     static_cast<ValueType::TSize>(1 << 5    ))
VALUE_TYPE_BIT(IntCanBeUntagged,        static_cast<ValueType::TSize>(1 << 6    ))
VALUE_TYPE_BIT(IntIsLikelyUntagged,     static_cast<ValueType::TSize>(1 << 7    ))
VALUE_TYPE_BIT(Float,                   static_cast<ValueType::TSize>(1 << 8    ))
VALUE_TYPE_BIT(Number,                  static_cast<ValueType::TSize>(1 << 9    ))
VALUE_TYPE_BIT(Boolean,                 static_cast<ValueType::TSize>(1 << 10   ))
VALUE_TYPE_BIT(String,                  static_cast<ValueType::TSize>(1 << 11   ))
VALUE_TYPE_BIT(Symbol,                  static_cast<ValueType::TSize>(1 << 12   ))
VALUE_TYPE_BIT(PrimitiveOrObject,       static_cast<ValueType::TSize>(1 << 13   ))

#if !defined(VALUE_TYPE_NONOBJECT_BIT_COUNT)
#define VALUE_TYPE_NONOBJECT_BIT_COUNT static_cast<ValueType::TSize>(9)
#endif

// The following bits only apply when the Object bit is set
VALUE_TYPE_BIT(NoMissingValues,         static_cast<ValueType::TSize>(1 << 5    )) // array
VALUE_TYPE_BIT(NonInts,                 static_cast<ValueType::TSize>(1 << 6    )) // array
VALUE_TYPE_BIT(NonFloats,               static_cast<ValueType::TSize>(1 << 7    )) // array

#if !defined(VALUE_TYPE_ARRAY_BIT_COUNT)
#define VALUE_TYPE_ARRAY_BIT_COUNT static_cast<ValueType::TSize>(3)
#endif

#if !defined(VALUE_TYPE_OBJECT_BIT_COUNT)
#define VALUE_TYPE_OBJECT_BIT_COUNT static_cast<ValueType::TSize>(3)
#endif

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Object types

#ifdef OBJECT_TYPE

OBJECT_TYPE(UninitializedObject )
OBJECT_TYPE(Object              )
// TODO (jedmiad): Add JsFunction to this list, once we have an optimization that can take advantage of it.

// Other objects
OBJECT_TYPE(RegExp              )

// Optimized arrays
OBJECT_TYPE(ObjectWithArray     )
OBJECT_TYPE(Array               )

// Typed arrays

// Typed arrays that are optimized by the JIT
OBJECT_TYPE(Int8Array           )
OBJECT_TYPE(Uint8Array          )
OBJECT_TYPE(Uint8ClampedArray   )
OBJECT_TYPE(Int16Array          )
OBJECT_TYPE(Uint16Array         )
OBJECT_TYPE(Int32Array          )
OBJECT_TYPE(Uint32Array         )
OBJECT_TYPE(Float32Array        )
OBJECT_TYPE(Float64Array        )

// Virtual Arrays
OBJECT_TYPE(Int8VirtualArray)
OBJECT_TYPE(Uint8VirtualArray)
OBJECT_TYPE(Uint8ClampedVirtualArray)
OBJECT_TYPE(Int16VirtualArray)
OBJECT_TYPE(Uint16VirtualArray)
OBJECT_TYPE(Int32VirtualArray)
OBJECT_TYPE(Uint32VirtualArray)
OBJECT_TYPE(Float32VirtualArray)
OBJECT_TYPE(Float64VirtualArray)

//Mixed Arrays
OBJECT_TYPE(Int8MixedArray)
OBJECT_TYPE(Uint8MixedArray)
OBJECT_TYPE(Uint8ClampedMixedArray)
OBJECT_TYPE(Int16MixedArray)
OBJECT_TYPE(Uint16MixedArray)
OBJECT_TYPE(Int32MixedArray)
OBJECT_TYPE(Uint32MixedArray)
OBJECT_TYPE(Float32MixedArray)
OBJECT_TYPE(Float64MixedArray)

// Typed arrays that are not optimized by the JIT
OBJECT_TYPE(Int64Array)
OBJECT_TYPE(Uint64Array)
OBJECT_TYPE(BoolArray)
OBJECT_TYPE(CharArray)

// SIMD_JS
// Only Simd128 sub-types. Currently no need to track top Simd128 type
OBJECT_TYPE(Simd128Float32x4    )
OBJECT_TYPE(Simd128Int32x4      )
OBJECT_TYPE(Simd128Int8x16      )
OBJECT_TYPE(Simd128Float64x2    )

OBJECT_TYPE(Count)

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Base value types

#ifdef BASE_VALUE_TYPE

BASE_VALUE_TYPE(Uninitialized,          Bits::Likely | Bits::CanBeTaggedValue )
BASE_VALUE_TYPE(Int,                    Bits::Int | Bits::CanBeTaggedValue )
BASE_VALUE_TYPE(Float,                  Bits::Float | Bits::CanBeTaggedValue )
BASE_VALUE_TYPE(Number,                 Bits::Number | Bits::CanBeTaggedValue )
BASE_VALUE_TYPE(Undefined,              Bits::Undefined             )
BASE_VALUE_TYPE(Null,                   Bits::Null                  )
BASE_VALUE_TYPE(Boolean,                Bits::Boolean               )
BASE_VALUE_TYPE(String,                 Bits::String                )
BASE_VALUE_TYPE(Symbol,                 Bits::Symbol                )
BASE_VALUE_TYPE(UninitializedObject,    Bits::Object                )
BASE_VALUE_TYPE(PrimitiveOrObject,      Bits::PrimitiveOrObject     )

#endif

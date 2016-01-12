//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// Runtime.h has both definitions and #include other runtime files.
// Definitions here are extracted definitions (not #include's) from Runtime.h that core Runtime .h's can be used without #including full Runtime.h

extern int TotalNumberOfBuiltInProperties;

namespace Js
{
    // Forwards
    class RecyclableObject;
    struct CallInfo;
    class PropertyRecord;
    class JavascriptString;
    struct FrameDisplay;
    class TypedArrayBase;

#if _M_IX86
#define unaligned
#elif _M_X64 || _M_ARM || _M_ARM64
#define unaligned __unaligned
#else
#error Must define alignment capabilities for processor
#endif

    typedef uint32 RegSlot;
    typedef uint16 ArgSlot;
    typedef uint16 PropertyIndex;
    typedef int32 BigPropertyIndex;
    typedef unsigned char PropertyAttributes;
    typedef uint32 SourceId;
    typedef uint16 ProfileId;
    typedef uint32 InlineCacheIndex;

    // Inline cache flags when property of the object is not writable
    enum InlineCacheFlags : char {
        InlineCacheNoFlags              = 0x0,
        InlineCacheGetterFlag           = 0x1,
        InlineCacheSetterFlag           = 0x2,
    };

    #define PropertyNone            0x00
    #define PropertyEnumerable      0x01
    #define PropertyConfigurable    0x02
    #define PropertyWritable        0x04
    #define PropertyDeleted         0x08
    #define PropertyLetConstGlobal  0x10
    #define PropertyDeclaredGlobal  0x20
    #define PropertyLet             0x40
    #define PropertyConst           0x80
    // No more flags will fit unless PropertyAttributes is bumped up to a short instead of char
    #define PropertyBuiltInMethodDefaults (PropertyConfigurable|PropertyWritable)
    #define PropertyDynamicTypeDefaults (PropertyConfigurable|PropertyWritable|PropertyEnumerable)
    #define PropertyLetDefaults   (PropertyEnumerable|PropertyConfigurable|PropertyWritable|PropertyLet)
    #define PropertyConstDefaults (PropertyEnumerable|PropertyConfigurable|PropertyConst)
    #define PropertyDeletedDefaults (PropertyDeleted | PropertyWritable | PropertyConfigurable)
    #define PropertyNoRedecl        (PropertyLet | PropertyConst)
    #define PropertyClassMemberDefaults (PropertyConfigurable|PropertyWritable)

    BEGIN_ENUM_UINT(InternalPropertyIds)
#define INTERNALPROPERTY(n) n,
#include "InternalPropertyList.h"
        Count,
    END_ENUM_UINT()

    inline BOOL IsInternalPropertyId(PropertyId propertyId)
    {
        return propertyId < InternalPropertyIds::Count;
    }

    BEGIN_ENUM_UINT(PropertyIds)
        _none = InternalPropertyIds::Count,
#define ENTRY_INTERNAL_SYMBOL(n) n,
#define ENTRY_SYMBOL(n, d) n,
#define ENTRY(n) n,
#define ENTRY2(n, s) n,
#include "Base\JnDirectFields.h"
        _countJSOnlyProperty,
    END_ENUM_UINT()

    inline BOOL IsBuiltInPropertyId(PropertyId propertyId)
    {
        return propertyId < TotalNumberOfBuiltInProperties;
    }

    #define PropertyTypesNone                      0x00
    #define PropertyTypesReserved                  0x01  // This bit is always to prevent the DWORD in DynamicTypeHandler looking like a pointer.
    #define PropertyTypesWritableDataOnly          0x10  // Indicates that a type handler has only writable data properties
                                                         // (no accessors or non-writable properties)
    #define PropertyTypesWritableDataOnlyDetection 0x20  // Set on each call to DynamicTypeHandler::SetHasOnlyWritableDataProperties.
    #define PropertyTypesInlineSlotCapacityLocked  0x40  // Indicates that the inline slot capacity has been shrunk already and shouldn't be touched again.
    #define PropertyTypesAll                       0x70
    typedef unsigned char PropertyTypes;                 // Holds flags that represent general information about the types of properties
                                                         // handled by a type handler.
    BEGIN_ENUM_UINT(JavascriptHint)
        None,                                   // no hint. use the default for that object
        HintString  = 0x00000001,               // 'string' hint in ToPrimitiveValue()
        HintNumber  = 0x00000002,               // 'number' hint
    END_ENUM_UINT()

    enum DescriptorFlags
    {
        None = 0x0,      // No data/accessor descriptor
        Accessor = 0x1,  // An accessor descriptor is present
        Data = 0x2,      // A data descriptor is present
        Writable = 0x4,  // Data descriptor is writable
        Const = 0x8,     // Data is const, meaning we throw on attempt to write to it
        Proxy = 0x10,    // data returned from proxy.
        WritableData = Data | Writable // Data descriptor is writable
    };

    BEGIN_ENUM_BYTE(BuiltinFunction)
#define LIBRARY_FUNCTION(obj, name, argc, flags, entry) obj##_##name,
#include "LibraryFunction.h"
#undef LIBRARY_FUNCTION
        Count,
        None,
    END_ENUM_BYTE()

    typedef void * Var;
    typedef WriteBarrierPtr<void> WriteBarrierVar;

    typedef Var(__cdecl *JavascriptMethod)(RecyclableObject*, CallInfo, ...);
    typedef Var(*ExternalMethod)(RecyclableObject*, CallInfo, Var*);


    const uintptr AtomTag_Object    = 0x0;

#if INT32VAR
    // The 49th bit is set in this representation
    const int32 VarTag_Shift        = 48;
    const uintptr AtomTag_IntPtr    = (((uintptr)0x1i64) << VarTag_Shift);
    const int32 AtomTag_Int32       = 0x0;     // lower 32-bits of a tagged integer
    const uintptr AtomTag           = 0x1;
    const int32 AtomTag_Multiply    = 1;
    const int32 AtomTag_Pair        = 0x00010001;  // Pair of tags
#else
    const uintptr AtomTag_IntPtr     = 0x1;
    const int32 AtomTag_Int32        = 0x1;    // lower 32-bits of a tagged integer
    const uintptr AtomTag            = 0x1;
    const int32 VarTag_Shift         = 1;
    const int32 AtomTag_Multiply     = 1 << VarTag_Shift;
#endif

#if FLOATVAR
    const uint64 FloatTag_Value      = 0xFFFCull << 48;
#endif
    template <bool IsPrototypeTemplate> class NullTypeHandler;

    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported> class SimpleDictionaryTypeHandlerBase;
    template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported> class SimpleDictionaryUnorderedTypeHandler;
    template <typename TPropertyIndex> class DictionaryTypeHandlerBase;
    template <typename TPropertyIndex> class ES5ArrayTypeHandlerBase;

    typedef NullTypeHandler<false> NonProtoNullTypeHandler;
    typedef NullTypeHandler<true> ProtoNullTypeHandler;

    typedef SimpleDictionaryTypeHandlerBase<PropertyIndex, const PropertyRecord*, false>    SimpleDictionaryTypeHandler;
    typedef SimpleDictionaryTypeHandlerBase<PropertyIndex, const PropertyRecord*, true>     SimpleDictionaryTypeHandlerNotExtensible;
    typedef SimpleDictionaryTypeHandlerBase<BigPropertyIndex, const PropertyRecord*, false> BigSimpleDictionaryTypeHandler;
    typedef SimpleDictionaryTypeHandlerBase<BigPropertyIndex, const PropertyRecord*, true>  BigSimpleDictionaryTypeHandlerNotExtensible;

    typedef SimpleDictionaryUnorderedTypeHandler<PropertyIndex, const PropertyRecord*, false>    SimpleDictionaryUnorderedPropertyRecordKeyedTypeHandler;
    typedef SimpleDictionaryUnorderedTypeHandler<PropertyIndex, const PropertyRecord*, true>     SimpleDictionaryUnorderedPropertyRecordKeyedTypeHandlerNotExtensible;
    typedef SimpleDictionaryUnorderedTypeHandler<BigPropertyIndex, const PropertyRecord*, false> BigSimpleDictionaryUnorderedPropertyRecordKeyedTypeHandler;
    typedef SimpleDictionaryUnorderedTypeHandler<BigPropertyIndex, const PropertyRecord*, true>  BigSimpleDictionaryUnorderedPropertyRecordKeyedTypeHandlerNotExtensible;

    typedef SimpleDictionaryUnorderedTypeHandler<PropertyIndex, JavascriptString*, false>    SimpleDictionaryUnorderedStringKeyedTypeHandler;
    typedef SimpleDictionaryUnorderedTypeHandler<PropertyIndex, JavascriptString*, true>     SimpleDictionaryUnorderedStringKeyedTypeHandlerNotExtensible;
    typedef SimpleDictionaryUnorderedTypeHandler<BigPropertyIndex, JavascriptString*, false> BigSimpleDictionaryUnorderedStringKeyedTypeHandler;
    typedef SimpleDictionaryUnorderedTypeHandler<BigPropertyIndex, JavascriptString*, true>  BigSimpleDictionaryUnorderedStringKeyedTypeHandlerNotExtensible;

    typedef DictionaryTypeHandlerBase<PropertyIndex> DictionaryTypeHandler;
    typedef DictionaryTypeHandlerBase<BigPropertyIndex> BigDictionaryTypeHandler;

    typedef ES5ArrayTypeHandlerBase<PropertyIndex> ES5ArrayTypeHandler;
    typedef ES5ArrayTypeHandlerBase<BigPropertyIndex> BigES5ArrayTypeHandler;

    template <int N> class ConcatStringN;
    typedef ConcatStringN<2> ConcatStringN2;
    typedef ConcatStringN<4> ConcatStringN4;
    typedef ConcatStringN<6> ConcatStringN6;
    typedef ConcatStringN<7> ConcatStringN7;

    template <wchar_t L, wchar_t R> class ConcatStringWrapping;
    typedef ConcatStringWrapping<L'[', L']'> ConcatStringWrappingSB;
    typedef ConcatStringWrapping<L'{', L'}'> ConcatStringWrappingB;
    typedef ConcatStringWrapping<L'"', L'"'> ConcatStringWrappingQ;

} // namespace Js.

namespace JSON
{
    class JSONParser;
}

//
// Below was moved from ByteCodeGenerator.h to share with jscript9diag.
//
#define REGSLOT_TO_VARREG(r) (r)
// To map between real reg number and const reg number, add 2 and negate.
// This way, 0xFFFF (no register) maps to itself, and 0xFFFF is never a valid number.
#define REGSLOT_TO_CONSTREG(r) ((Js::RegSlot)(0 - (r + 2)))
#define CONSTREG_TO_REGSLOT(r) ((Js::RegSlot)(0 - (r + 2)))

//
// Shared string literals
//
#define JS_DISPLAY_STRING_NAN           L"NaN"
#define JS_DISPLAY_STRING_DATE          L"Date"
#define JS_DISPLAY_STRING_INVALID_DATE  L"Invalid Date"
#define JS_DISPLAY_STRING_FUNCTION_ANONYMOUS        L"\012function() {\012    [native code]\012}\012"
#define JS_DISPLAY_STRING_FUNCTION_HEADER           L"function "
#define JS_DISPLAY_STRING_FUNCTION_BODY             L"() { [native code] }"

#define JS_DIAG_TYPE_JavascriptRegExp               L"Object, (Regular Expression)"

#define JS_DIAG_VALUE_JavascriptRegExpConstructor   L"{...}"
#define JS_DIAG_TYPE_JavascriptRegExpConstructor    L"Object, (RegExp constructor)"

#define JS_DEFAULT_CTOR_DISPLAY_STRING              L"constructor() {}"
#define JS_DEFAULT_EXTENDS_CTOR_DISPLAY_STRING      L"constructor(...args) { super(...args); }"


#include "Language\SIMDUtils.h"


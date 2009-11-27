// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_OBJECTS_H_
#define V8_OBJECTS_H_

#include "builtins.h"
#include "code-stubs.h"
#include "smart-pointer.h"
#include "unicode-inl.h"
#if V8_TARGET_ARCH_ARM
#include "arm/constants-arm.h"
#endif

//
// All object types in the V8 JavaScript are described in this file.
//
// Inheritance hierarchy:
//   - Object
//     - Smi          (immediate small integer)
//     - Failure      (immediate for marking failed operation)
//     - HeapObject   (superclass for everything allocated in the heap)
//       - JSObject
//         - JSArray
//         - JSRegExp
//         - JSFunction
//         - GlobalObject
//           - JSGlobalObject
//           - JSBuiltinsObject
//         - JSGlobalProxy
//         - JSValue
//       - Array
//         - ByteArray
//         - PixelArray
//         - ExternalArray
//           - ExternalByteArray
//           - ExternalUnsignedByteArray
//           - ExternalShortArray
//           - ExternalUnsignedShortArray
//           - ExternalIntArray
//           - ExternalUnsignedIntArray
//           - ExternalFloatArray
//         - FixedArray
//           - DescriptorArray
//           - HashTable
//             - Dictionary
//             - SymbolTable
//             - CompilationCacheTable
//             - MapCache
//           - Context
//           - GlobalContext
//       - String
//         - SeqString
//           - SeqAsciiString
//           - SeqTwoByteString
//         - ConsString
//         - ExternalString
//           - ExternalAsciiString
//           - ExternalTwoByteString
//       - HeapNumber
//       - Code
//       - Map
//       - Oddball
//       - Proxy
//       - SharedFunctionInfo
//       - Struct
//         - AccessorInfo
//         - AccessCheckInfo
//         - InterceptorInfo
//         - CallHandlerInfo
//         - TemplateInfo
//           - FunctionTemplateInfo
//           - ObjectTemplateInfo
//         - Script
//         - SignatureInfo
//         - TypeSwitchInfo
//         - DebugInfo
//         - BreakPointInfo
//
// Formats of Object*:
//  Smi:        [31 bit signed int] 0
//  HeapObject: [32 bit direct pointer] (4 byte aligned) | 01
//  Failure:    [30 bit signed int] 11

// Ecma-262 3rd 8.6.1
enum PropertyAttributes {
  NONE              = v8::None,
  READ_ONLY         = v8::ReadOnly,
  DONT_ENUM         = v8::DontEnum,
  DONT_DELETE       = v8::DontDelete,
  ABSENT            = 16  // Used in runtime to indicate a property is absent.
  // ABSENT can never be stored in or returned from a descriptor's attributes
  // bitfield.  It is only used as a return value meaning the attributes of
  // a non-existent property.
};

namespace v8 {
namespace internal {


// PropertyDetails captures type and attributes for a property.
// They are used both in property dictionaries and instance descriptors.
class PropertyDetails BASE_EMBEDDED {
 public:

  PropertyDetails(PropertyAttributes attributes,
                  PropertyType type,
                  int index = 0) {
    ASSERT(TypeField::is_valid(type));
    ASSERT(AttributesField::is_valid(attributes));
    ASSERT(IndexField::is_valid(index));

    value_ = TypeField::encode(type)
        | AttributesField::encode(attributes)
        | IndexField::encode(index);

    ASSERT(type == this->type());
    ASSERT(attributes == this->attributes());
    ASSERT(index == this->index());
  }

  // Conversion for storing details as Object*.
  inline PropertyDetails(Smi* smi);
  inline Smi* AsSmi();

  PropertyType type() { return TypeField::decode(value_); }

  bool IsTransition() {
    PropertyType t = type();
    ASSERT(t != INTERCEPTOR);
    return t == MAP_TRANSITION || t == CONSTANT_TRANSITION;
  }

  bool IsProperty() {
    return type() < FIRST_PHANTOM_PROPERTY_TYPE;
  }

  PropertyAttributes attributes() { return AttributesField::decode(value_); }

  int index() { return IndexField::decode(value_); }

  inline PropertyDetails AsDeleted();

  static bool IsValidIndex(int index) { return IndexField::is_valid(index); }

  bool IsReadOnly() { return (attributes() & READ_ONLY) != 0; }
  bool IsDontDelete() { return (attributes() & DONT_DELETE) != 0; }
  bool IsDontEnum() { return (attributes() & DONT_ENUM) != 0; }
  bool IsDeleted() { return DeletedField::decode(value_) != 0;}

  // Bit fields in value_ (type, shift, size). Must be public so the
  // constants can be embedded in generated code.
  class TypeField:       public BitField<PropertyType,       0, 3> {};
  class AttributesField: public BitField<PropertyAttributes, 3, 3> {};
  class DeletedField:    public BitField<uint32_t,           6, 1> {};
  class IndexField:      public BitField<uint32_t,           7, 31-7> {};

  static const int kInitialIndex = 1;
 private:
  uint32_t value_;
};


// Setter that skips the write barrier if mode is SKIP_WRITE_BARRIER.
enum WriteBarrierMode { SKIP_WRITE_BARRIER, UPDATE_WRITE_BARRIER };


// PropertyNormalizationMode is used to specify whether to keep
// inobject properties when normalizing properties of a JSObject.
enum PropertyNormalizationMode {
  CLEAR_INOBJECT_PROPERTIES,
  KEEP_INOBJECT_PROPERTIES
};


// All Maps have a field instance_type containing a InstanceType.
// It describes the type of the instances.
//
// As an example, a JavaScript object is a heap object and its map
// instance_type is JS_OBJECT_TYPE.
//
// The names of the string instance types are intended to systematically
// mirror their encoding in the instance_type field of the map.  The length
// (SHORT, MEDIUM, or LONG) is always mentioned.  The default encoding is
// considered TWO_BYTE.  It is not mentioned in the name.  ASCII encoding is
// mentioned explicitly in the name.  Likewise, the default representation is
// considered sequential.  It is not mentioned in the name.  The other
// representations (eg, CONS, EXTERNAL) are explicitly mentioned.
// Finally, the string is either a SYMBOL_TYPE (if it is a symbol) or a
// STRING_TYPE (if it is not a symbol).
//
// NOTE: The following things are some that depend on the string types having
// instance_types that are less than those of all other types:
// HeapObject::Size, HeapObject::IterateBody, the typeof operator, and
// Object::IsString.
//
// NOTE: Everything following JS_VALUE_TYPE is considered a
// JSObject for GC purposes. The first four entries here have typeof
// 'object', whereas JS_FUNCTION_TYPE has typeof 'function'.
#define INSTANCE_TYPE_LIST_ALL(V)               \
  V(SHORT_SYMBOL_TYPE)                          \
  V(MEDIUM_SYMBOL_TYPE)                         \
  V(LONG_SYMBOL_TYPE)                           \
  V(SHORT_ASCII_SYMBOL_TYPE)                    \
  V(MEDIUM_ASCII_SYMBOL_TYPE)                   \
  V(LONG_ASCII_SYMBOL_TYPE)                     \
  V(SHORT_CONS_SYMBOL_TYPE)                     \
  V(MEDIUM_CONS_SYMBOL_TYPE)                    \
  V(LONG_CONS_SYMBOL_TYPE)                      \
  V(SHORT_CONS_ASCII_SYMBOL_TYPE)               \
  V(MEDIUM_CONS_ASCII_SYMBOL_TYPE)              \
  V(LONG_CONS_ASCII_SYMBOL_TYPE)                \
  V(SHORT_EXTERNAL_SYMBOL_TYPE)                 \
  V(MEDIUM_EXTERNAL_SYMBOL_TYPE)                \
  V(LONG_EXTERNAL_SYMBOL_TYPE)                  \
  V(SHORT_EXTERNAL_ASCII_SYMBOL_TYPE)           \
  V(MEDIUM_EXTERNAL_ASCII_SYMBOL_TYPE)          \
  V(LONG_EXTERNAL_ASCII_SYMBOL_TYPE)            \
  V(SHORT_STRING_TYPE)                          \
  V(MEDIUM_STRING_TYPE)                         \
  V(LONG_STRING_TYPE)                           \
  V(SHORT_ASCII_STRING_TYPE)                    \
  V(MEDIUM_ASCII_STRING_TYPE)                   \
  V(LONG_ASCII_STRING_TYPE)                     \
  V(SHORT_CONS_STRING_TYPE)                     \
  V(MEDIUM_CONS_STRING_TYPE)                    \
  V(LONG_CONS_STRING_TYPE)                      \
  V(SHORT_CONS_ASCII_STRING_TYPE)               \
  V(MEDIUM_CONS_ASCII_STRING_TYPE)              \
  V(LONG_CONS_ASCII_STRING_TYPE)                \
  V(SHORT_EXTERNAL_STRING_TYPE)                 \
  V(MEDIUM_EXTERNAL_STRING_TYPE)                \
  V(LONG_EXTERNAL_STRING_TYPE)                  \
  V(SHORT_EXTERNAL_ASCII_STRING_TYPE)           \
  V(MEDIUM_EXTERNAL_ASCII_STRING_TYPE)          \
  V(LONG_EXTERNAL_ASCII_STRING_TYPE)            \
  V(LONG_PRIVATE_EXTERNAL_ASCII_STRING_TYPE)    \
                                                \
  V(MAP_TYPE)                                   \
  V(HEAP_NUMBER_TYPE)                           \
  V(FIXED_ARRAY_TYPE)                           \
  V(CODE_TYPE)                                  \
  V(JS_GLOBAL_PROPERTY_CELL_TYPE)               \
  V(ODDBALL_TYPE)                               \
  V(PROXY_TYPE)                                 \
  V(BYTE_ARRAY_TYPE)                            \
  V(PIXEL_ARRAY_TYPE)                           \
  /* Note: the order of these external array */ \
  /* types is relied upon in */                 \
  /* Object::IsExternalArray(). */              \
  V(EXTERNAL_BYTE_ARRAY_TYPE)                   \
  V(EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE)          \
  V(EXTERNAL_SHORT_ARRAY_TYPE)                  \
  V(EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE)         \
  V(EXTERNAL_INT_ARRAY_TYPE)                    \
  V(EXTERNAL_UNSIGNED_INT_ARRAY_TYPE)           \
  V(EXTERNAL_FLOAT_ARRAY_TYPE)                  \
  V(FILLER_TYPE)                                \
                                                \
  V(ACCESSOR_INFO_TYPE)                         \
  V(ACCESS_CHECK_INFO_TYPE)                     \
  V(INTERCEPTOR_INFO_TYPE)                      \
  V(SHARED_FUNCTION_INFO_TYPE)                  \
  V(CALL_HANDLER_INFO_TYPE)                     \
  V(FUNCTION_TEMPLATE_INFO_TYPE)                \
  V(OBJECT_TEMPLATE_INFO_TYPE)                  \
  V(SIGNATURE_INFO_TYPE)                        \
  V(TYPE_SWITCH_INFO_TYPE)                      \
  V(SCRIPT_TYPE)                                \
                                                \
  V(JS_VALUE_TYPE)                              \
  V(JS_OBJECT_TYPE)                             \
  V(JS_CONTEXT_EXTENSION_OBJECT_TYPE)           \
  V(JS_GLOBAL_OBJECT_TYPE)                      \
  V(JS_BUILTINS_OBJECT_TYPE)                    \
  V(JS_GLOBAL_PROXY_TYPE)                       \
  V(JS_ARRAY_TYPE)                              \
  V(JS_REGEXP_TYPE)                             \
                                                \
  V(JS_FUNCTION_TYPE)                           \

#ifdef ENABLE_DEBUGGER_SUPPORT
#define INSTANCE_TYPE_LIST_DEBUGGER(V)          \
  V(DEBUG_INFO_TYPE)                            \
  V(BREAK_POINT_INFO_TYPE)
#else
#define INSTANCE_TYPE_LIST_DEBUGGER(V)
#endif

#define INSTANCE_TYPE_LIST(V)                   \
  INSTANCE_TYPE_LIST_ALL(V)                     \
  INSTANCE_TYPE_LIST_DEBUGGER(V)


// Since string types are not consecutive, this macro is used to
// iterate over them.
#define STRING_TYPE_LIST(V)                                                    \
  V(SHORT_SYMBOL_TYPE,                                                         \
    SeqTwoByteString::kAlignedSize,                                            \
    short_symbol,                                                              \
    ShortSymbol)                                                               \
  V(MEDIUM_SYMBOL_TYPE,                                                        \
    SeqTwoByteString::kAlignedSize,                                            \
    medium_symbol,                                                             \
    MediumSymbol)                                                              \
  V(LONG_SYMBOL_TYPE,                                                          \
    SeqTwoByteString::kAlignedSize,                                            \
    long_symbol,                                                               \
    LongSymbol)                                                                \
  V(SHORT_ASCII_SYMBOL_TYPE,                                                   \
    SeqAsciiString::kAlignedSize,                                              \
    short_ascii_symbol,                                                        \
    ShortAsciiSymbol)                                                          \
  V(MEDIUM_ASCII_SYMBOL_TYPE,                                                  \
    SeqAsciiString::kAlignedSize,                                              \
    medium_ascii_symbol,                                                       \
    MediumAsciiSymbol)                                                         \
  V(LONG_ASCII_SYMBOL_TYPE,                                                    \
    SeqAsciiString::kAlignedSize,                                              \
    long_ascii_symbol,                                                         \
    LongAsciiSymbol)                                                           \
  V(SHORT_CONS_SYMBOL_TYPE,                                                    \
    ConsString::kSize,                                                         \
    short_cons_symbol,                                                         \
    ShortConsSymbol)                                                           \
  V(MEDIUM_CONS_SYMBOL_TYPE,                                                   \
    ConsString::kSize,                                                         \
    medium_cons_symbol,                                                        \
    MediumConsSymbol)                                                          \
  V(LONG_CONS_SYMBOL_TYPE,                                                     \
    ConsString::kSize,                                                         \
    long_cons_symbol,                                                          \
    LongConsSymbol)                                                            \
  V(SHORT_CONS_ASCII_SYMBOL_TYPE,                                              \
    ConsString::kSize,                                                         \
    short_cons_ascii_symbol,                                                   \
    ShortConsAsciiSymbol)                                                      \
  V(MEDIUM_CONS_ASCII_SYMBOL_TYPE,                                             \
    ConsString::kSize,                                                         \
    medium_cons_ascii_symbol,                                                  \
    MediumConsAsciiSymbol)                                                     \
  V(LONG_CONS_ASCII_SYMBOL_TYPE,                                               \
    ConsString::kSize,                                                         \
    long_cons_ascii_symbol,                                                    \
    LongConsAsciiSymbol)                                                       \
  V(SHORT_EXTERNAL_SYMBOL_TYPE,                                                \
    ExternalTwoByteString::kSize,                                              \
    short_external_symbol,                                                     \
    ShortExternalSymbol)                                                       \
  V(MEDIUM_EXTERNAL_SYMBOL_TYPE,                                               \
    ExternalTwoByteString::kSize,                                              \
    medium_external_symbol,                                                    \
    MediumExternalSymbol)                                                      \
  V(LONG_EXTERNAL_SYMBOL_TYPE,                                                 \
    ExternalTwoByteString::kSize,                                              \
    long_external_symbol,                                                      \
    LongExternalSymbol)                                                        \
  V(SHORT_EXTERNAL_ASCII_SYMBOL_TYPE,                                          \
    ExternalAsciiString::kSize,                                                \
    short_external_ascii_symbol,                                               \
    ShortExternalAsciiSymbol)                                                  \
  V(MEDIUM_EXTERNAL_ASCII_SYMBOL_TYPE,                                         \
    ExternalAsciiString::kSize,                                                \
    medium_external_ascii_symbol,                                              \
    MediumExternalAsciiSymbol)                                                 \
  V(LONG_EXTERNAL_ASCII_SYMBOL_TYPE,                                           \
    ExternalAsciiString::kSize,                                                \
    long_external_ascii_symbol,                                                \
    LongExternalAsciiSymbol)                                                   \
  V(SHORT_STRING_TYPE,                                                         \
    SeqTwoByteString::kAlignedSize,                                            \
    short_string,                                                              \
    ShortString)                                                               \
  V(MEDIUM_STRING_TYPE,                                                        \
    SeqTwoByteString::kAlignedSize,                                            \
    medium_string,                                                             \
    MediumString)                                                              \
  V(LONG_STRING_TYPE,                                                          \
    SeqTwoByteString::kAlignedSize,                                            \
    long_string,                                                               \
    LongString)                                                                \
  V(SHORT_ASCII_STRING_TYPE,                                                   \
    SeqAsciiString::kAlignedSize,                                              \
    short_ascii_string,                                                        \
    ShortAsciiString)                                                          \
  V(MEDIUM_ASCII_STRING_TYPE,                                                  \
    SeqAsciiString::kAlignedSize,                                              \
    medium_ascii_string,                                                       \
    MediumAsciiString)                                                         \
  V(LONG_ASCII_STRING_TYPE,                                                    \
    SeqAsciiString::kAlignedSize,                                              \
    long_ascii_string,                                                         \
    LongAsciiString)                                                           \
  V(SHORT_CONS_STRING_TYPE,                                                    \
    ConsString::kSize,                                                         \
    short_cons_string,                                                         \
    ShortConsString)                                                           \
  V(MEDIUM_CONS_STRING_TYPE,                                                   \
    ConsString::kSize,                                                         \
    medium_cons_string,                                                        \
    MediumConsString)                                                          \
  V(LONG_CONS_STRING_TYPE,                                                     \
    ConsString::kSize,                                                         \
    long_cons_string,                                                          \
    LongConsString)                                                            \
  V(SHORT_CONS_ASCII_STRING_TYPE,                                              \
    ConsString::kSize,                                                         \
    short_cons_ascii_string,                                                   \
    ShortConsAsciiString)                                                      \
  V(MEDIUM_CONS_ASCII_STRING_TYPE,                                             \
    ConsString::kSize,                                                         \
    medium_cons_ascii_string,                                                  \
    MediumConsAsciiString)                                                     \
  V(LONG_CONS_ASCII_STRING_TYPE,                                               \
    ConsString::kSize,                                                         \
    long_cons_ascii_string,                                                    \
    LongConsAsciiString)                                                       \
  V(SHORT_EXTERNAL_STRING_TYPE,                                                \
    ExternalTwoByteString::kSize,                                              \
    short_external_string,                                                     \
    ShortExternalString)                                                       \
  V(MEDIUM_EXTERNAL_STRING_TYPE,                                               \
    ExternalTwoByteString::kSize,                                              \
    medium_external_string,                                                    \
    MediumExternalString)                                                      \
  V(LONG_EXTERNAL_STRING_TYPE,                                                 \
    ExternalTwoByteString::kSize,                                              \
    long_external_string,                                                      \
    LongExternalString)                                                        \
  V(SHORT_EXTERNAL_ASCII_STRING_TYPE,                                          \
    ExternalAsciiString::kSize,                                                \
    short_external_ascii_string,                                               \
    ShortExternalAsciiString)                                                  \
  V(MEDIUM_EXTERNAL_ASCII_STRING_TYPE,                                         \
    ExternalAsciiString::kSize,                                                \
    medium_external_ascii_string,                                              \
    MediumExternalAsciiString)                                                 \
  V(LONG_EXTERNAL_ASCII_STRING_TYPE,                                           \
    ExternalAsciiString::kSize,                                                \
    long_external_ascii_string,                                                \
    LongExternalAsciiString)

// A struct is a simple object a set of object-valued fields.  Including an
// object type in this causes the compiler to generate most of the boilerplate
// code for the class including allocation and garbage collection routines,
// casts and predicates.  All you need to define is the class, methods and
// object verification routines.  Easy, no?
//
// Note that for subtle reasons related to the ordering or numerical values of
// type tags, elements in this list have to be added to the INSTANCE_TYPE_LIST
// manually.
#define STRUCT_LIST_ALL(V)                                                \
  V(ACCESSOR_INFO, AccessorInfo, accessor_info)                           \
  V(ACCESS_CHECK_INFO, AccessCheckInfo, access_check_info)                \
  V(INTERCEPTOR_INFO, InterceptorInfo, interceptor_info)                  \
  V(CALL_HANDLER_INFO, CallHandlerInfo, call_handler_info)                \
  V(FUNCTION_TEMPLATE_INFO, FunctionTemplateInfo, function_template_info) \
  V(OBJECT_TEMPLATE_INFO, ObjectTemplateInfo, object_template_info)       \
  V(SIGNATURE_INFO, SignatureInfo, signature_info)                        \
  V(TYPE_SWITCH_INFO, TypeSwitchInfo, type_switch_info)                   \
  V(SCRIPT, Script, script)

#ifdef ENABLE_DEBUGGER_SUPPORT
#define STRUCT_LIST_DEBUGGER(V)                                           \
  V(DEBUG_INFO, DebugInfo, debug_info)                                    \
  V(BREAK_POINT_INFO, BreakPointInfo, break_point_info)
#else
#define STRUCT_LIST_DEBUGGER(V)
#endif

#define STRUCT_LIST(V)                                                    \
  STRUCT_LIST_ALL(V)                                                      \
  STRUCT_LIST_DEBUGGER(V)

// We use the full 8 bits of the instance_type field to encode heap object
// instance types.  The high-order bit (bit 7) is set if the object is not a
// string, and cleared if it is a string.
const uint32_t kIsNotStringMask = 0x80;
const uint32_t kStringTag = 0x0;
const uint32_t kNotStringTag = 0x80;

// If bit 7 is clear, bit 5 indicates that the string is a symbol (if set) or
// not (if cleared).
const uint32_t kIsSymbolMask = 0x20;
const uint32_t kNotSymbolTag = 0x0;
const uint32_t kSymbolTag = 0x20;

// If bit 7 is clear, bits 3 and 4 are the string's size (short, medium or
// long).  These values are very special in that they are also used to shift
// the length field to get the length, removing the hash value.  This avoids
// using if or switch when getting the length of a string.
const uint32_t kStringSizeMask = 0x18;
const uint32_t kShortStringTag = 0x18;
const uint32_t kMediumStringTag = 0x10;
const uint32_t kLongStringTag = 0x00;

// If bit 7 is clear then bit 2 indicates whether the string consists of
// two-byte characters or one-byte characters.
const uint32_t kStringEncodingMask = 0x4;
const uint32_t kTwoByteStringTag = 0x0;
const uint32_t kAsciiStringTag = 0x4;

// If bit 7 is clear, the low-order 2 bits indicate the representation
// of the string.
const uint32_t kStringRepresentationMask = 0x03;
enum StringRepresentationTag {
  kSeqStringTag = 0x0,
  kConsStringTag = 0x1,
  kExternalStringTag = 0x3
};


// A ConsString with an empty string as the right side is a candidate
// for being shortcut by the garbage collector unless it is a
// symbol. It's not common to have non-flat symbols, so we do not
// shortcut them thereby avoiding turning symbols into strings. See
// heap.cc and mark-compact.cc.
const uint32_t kShortcutTypeMask =
    kIsNotStringMask |
    kIsSymbolMask |
    kStringRepresentationMask;
const uint32_t kShortcutTypeTag = kConsStringTag;


enum InstanceType {
  SHORT_SYMBOL_TYPE = kShortStringTag | kSymbolTag | kSeqStringTag,
  MEDIUM_SYMBOL_TYPE = kMediumStringTag | kSymbolTag | kSeqStringTag,
  LONG_SYMBOL_TYPE = kLongStringTag | kSymbolTag | kSeqStringTag,
  SHORT_ASCII_SYMBOL_TYPE =
      kShortStringTag | kAsciiStringTag | kSymbolTag | kSeqStringTag,
  MEDIUM_ASCII_SYMBOL_TYPE =
      kMediumStringTag | kAsciiStringTag | kSymbolTag | kSeqStringTag,
  LONG_ASCII_SYMBOL_TYPE =
      kLongStringTag | kAsciiStringTag | kSymbolTag | kSeqStringTag,
  SHORT_CONS_SYMBOL_TYPE = kShortStringTag | kSymbolTag | kConsStringTag,
  MEDIUM_CONS_SYMBOL_TYPE = kMediumStringTag | kSymbolTag | kConsStringTag,
  LONG_CONS_SYMBOL_TYPE = kLongStringTag | kSymbolTag | kConsStringTag,
  SHORT_CONS_ASCII_SYMBOL_TYPE =
      kShortStringTag | kAsciiStringTag | kSymbolTag | kConsStringTag,
  MEDIUM_CONS_ASCII_SYMBOL_TYPE =
      kMediumStringTag | kAsciiStringTag | kSymbolTag | kConsStringTag,
  LONG_CONS_ASCII_SYMBOL_TYPE =
      kLongStringTag | kAsciiStringTag | kSymbolTag | kConsStringTag,
  SHORT_EXTERNAL_SYMBOL_TYPE =
      kShortStringTag | kSymbolTag | kExternalStringTag,
  MEDIUM_EXTERNAL_SYMBOL_TYPE =
      kMediumStringTag | kSymbolTag | kExternalStringTag,
  LONG_EXTERNAL_SYMBOL_TYPE = kLongStringTag | kSymbolTag | kExternalStringTag,
  SHORT_EXTERNAL_ASCII_SYMBOL_TYPE =
      kShortStringTag | kAsciiStringTag | kSymbolTag | kExternalStringTag,
  MEDIUM_EXTERNAL_ASCII_SYMBOL_TYPE =
      kMediumStringTag | kAsciiStringTag | kSymbolTag | kExternalStringTag,
  LONG_EXTERNAL_ASCII_SYMBOL_TYPE =
      kLongStringTag | kAsciiStringTag | kSymbolTag | kExternalStringTag,
  SHORT_STRING_TYPE = kShortStringTag | kSeqStringTag,
  MEDIUM_STRING_TYPE = kMediumStringTag | kSeqStringTag,
  LONG_STRING_TYPE = kLongStringTag | kSeqStringTag,
  SHORT_ASCII_STRING_TYPE = kShortStringTag | kAsciiStringTag | kSeqStringTag,
  MEDIUM_ASCII_STRING_TYPE = kMediumStringTag | kAsciiStringTag | kSeqStringTag,
  LONG_ASCII_STRING_TYPE = kLongStringTag | kAsciiStringTag | kSeqStringTag,
  SHORT_CONS_STRING_TYPE = kShortStringTag | kConsStringTag,
  MEDIUM_CONS_STRING_TYPE = kMediumStringTag | kConsStringTag,
  LONG_CONS_STRING_TYPE = kLongStringTag | kConsStringTag,
  SHORT_CONS_ASCII_STRING_TYPE =
      kShortStringTag | kAsciiStringTag | kConsStringTag,
  MEDIUM_CONS_ASCII_STRING_TYPE =
      kMediumStringTag | kAsciiStringTag | kConsStringTag,
  LONG_CONS_ASCII_STRING_TYPE =
      kLongStringTag | kAsciiStringTag | kConsStringTag,
  SHORT_EXTERNAL_STRING_TYPE = kShortStringTag | kExternalStringTag,
  MEDIUM_EXTERNAL_STRING_TYPE = kMediumStringTag | kExternalStringTag,
  LONG_EXTERNAL_STRING_TYPE = kLongStringTag | kExternalStringTag,
  SHORT_EXTERNAL_ASCII_STRING_TYPE =
      kShortStringTag | kAsciiStringTag | kExternalStringTag,
  MEDIUM_EXTERNAL_ASCII_STRING_TYPE =
      kMediumStringTag | kAsciiStringTag | kExternalStringTag,
  LONG_EXTERNAL_ASCII_STRING_TYPE =
      kLongStringTag | kAsciiStringTag | kExternalStringTag,
  LONG_PRIVATE_EXTERNAL_ASCII_STRING_TYPE = LONG_EXTERNAL_ASCII_STRING_TYPE,

  MAP_TYPE = kNotStringTag,
  HEAP_NUMBER_TYPE,
  FIXED_ARRAY_TYPE,
  CODE_TYPE,
  ODDBALL_TYPE,
  JS_GLOBAL_PROPERTY_CELL_TYPE,
  PROXY_TYPE,
  BYTE_ARRAY_TYPE,
  PIXEL_ARRAY_TYPE,
  EXTERNAL_BYTE_ARRAY_TYPE,
  EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE,
  EXTERNAL_SHORT_ARRAY_TYPE,
  EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE,
  EXTERNAL_INT_ARRAY_TYPE,
  EXTERNAL_UNSIGNED_INT_ARRAY_TYPE,
  EXTERNAL_FLOAT_ARRAY_TYPE,
  FILLER_TYPE,
  SMI_TYPE,

  ACCESSOR_INFO_TYPE,
  ACCESS_CHECK_INFO_TYPE,
  INTERCEPTOR_INFO_TYPE,
  SHARED_FUNCTION_INFO_TYPE,
  CALL_HANDLER_INFO_TYPE,
  FUNCTION_TEMPLATE_INFO_TYPE,
  OBJECT_TEMPLATE_INFO_TYPE,
  SIGNATURE_INFO_TYPE,
  TYPE_SWITCH_INFO_TYPE,
#ifdef ENABLE_DEBUGGER_SUPPORT
  DEBUG_INFO_TYPE,
  BREAK_POINT_INFO_TYPE,
#endif
  SCRIPT_TYPE,

  JS_VALUE_TYPE,
  JS_OBJECT_TYPE,
  JS_CONTEXT_EXTENSION_OBJECT_TYPE,
  JS_GLOBAL_OBJECT_TYPE,
  JS_BUILTINS_OBJECT_TYPE,
  JS_GLOBAL_PROXY_TYPE,
  JS_ARRAY_TYPE,
  JS_REGEXP_TYPE,

  JS_FUNCTION_TYPE,

  // Pseudo-types
  FIRST_NONSTRING_TYPE = MAP_TYPE,
  FIRST_TYPE = 0x0,
  INVALID_TYPE = FIRST_TYPE - 1,
  LAST_TYPE = JS_FUNCTION_TYPE,
  // Boundaries for testing the type is a JavaScript "object".  Note that
  // function objects are not counted as objects, even though they are
  // implemented as such; only values whose typeof is "object" are included.
  FIRST_JS_OBJECT_TYPE = JS_VALUE_TYPE,
  LAST_JS_OBJECT_TYPE = JS_REGEXP_TYPE
};


enum CompareResult {
  LESS      = -1,
  EQUAL     =  0,
  GREATER   =  1,

  NOT_EQUAL = GREATER
};


#define DECL_BOOLEAN_ACCESSORS(name)   \
  inline bool name();                  \
  inline void set_##name(bool value);  \


#define DECL_ACCESSORS(name, type)                                      \
  inline type* name();                                                  \
  inline void set_##name(type* value,                                   \
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER); \


class StringStream;
class ObjectVisitor;

struct ValueInfo : public Malloced {
  ValueInfo() : type(FIRST_TYPE), ptr(NULL), str(NULL), number(0) { }
  InstanceType type;
  Object* ptr;
  const char* str;
  double number;
};


// A template-ized version of the IsXXX functions.
template <class C> static inline bool Is(Object* obj);


// Object is the abstract superclass for all classes in the
// object hierarchy.
// Object does not use any virtual functions to avoid the
// allocation of the C++ vtable.
// Since Smi and Failure are subclasses of Object no
// data members can be present in Object.
class Object BASE_EMBEDDED {
 public:
  // Type testing.
  inline bool IsSmi();
  inline bool IsHeapObject();
  inline bool IsHeapNumber();
  inline bool IsString();
  inline bool IsSymbol();
  // See objects-inl.h for more details
  inline bool IsSeqString();
  inline bool IsExternalString();
  inline bool IsExternalTwoByteString();
  inline bool IsExternalAsciiString();
  inline bool IsSeqTwoByteString();
  inline bool IsSeqAsciiString();
  inline bool IsConsString();

  inline bool IsNumber();
  inline bool IsByteArray();
  inline bool IsPixelArray();
  inline bool IsExternalArray();
  inline bool IsExternalByteArray();
  inline bool IsExternalUnsignedByteArray();
  inline bool IsExternalShortArray();
  inline bool IsExternalUnsignedShortArray();
  inline bool IsExternalIntArray();
  inline bool IsExternalUnsignedIntArray();
  inline bool IsExternalFloatArray();
  inline bool IsFailure();
  inline bool IsRetryAfterGC();
  inline bool IsOutOfMemoryFailure();
  inline bool IsException();
  inline bool IsJSObject();
  inline bool IsJSContextExtensionObject();
  inline bool IsMap();
  inline bool IsFixedArray();
  inline bool IsDescriptorArray();
  inline bool IsContext();
  inline bool IsCatchContext();
  inline bool IsGlobalContext();
  inline bool IsJSFunction();
  inline bool IsCode();
  inline bool IsOddball();
  inline bool IsSharedFunctionInfo();
  inline bool IsJSValue();
  inline bool IsStringWrapper();
  inline bool IsProxy();
  inline bool IsBoolean();
  inline bool IsJSArray();
  inline bool IsJSRegExp();
  inline bool IsHashTable();
  inline bool IsDictionary();
  inline bool IsSymbolTable();
  inline bool IsCompilationCacheTable();
  inline bool IsMapCache();
  inline bool IsPrimitive();
  inline bool IsGlobalObject();
  inline bool IsJSGlobalObject();
  inline bool IsJSBuiltinsObject();
  inline bool IsJSGlobalProxy();
  inline bool IsUndetectableObject();
  inline bool IsAccessCheckNeeded();
  inline bool IsJSGlobalPropertyCell();

  // Returns true if this object is an instance of the specified
  // function template.
  inline bool IsInstanceOf(FunctionTemplateInfo* type);

  inline bool IsStruct();
#define DECLARE_STRUCT_PREDICATE(NAME, Name, name) inline bool Is##Name();
  STRUCT_LIST(DECLARE_STRUCT_PREDICATE)
#undef DECLARE_STRUCT_PREDICATE

  // Oddball testing.
  INLINE(bool IsUndefined());
  INLINE(bool IsTheHole());
  INLINE(bool IsNull());
  INLINE(bool IsTrue());
  INLINE(bool IsFalse());

  // Extract the number.
  inline double Number();

  inline bool HasSpecificClassOf(String* name);

  Object* ToObject();             // ECMA-262 9.9.
  Object* ToBoolean();            // ECMA-262 9.2.

  // Convert to a JSObject if needed.
  // global_context is used when creating wrapper object.
  Object* ToObject(Context* global_context);

  // Converts this to a Smi if possible.
  // Failure is returned otherwise.
  inline Object* ToSmi();

  void Lookup(String* name, LookupResult* result);

  // Property access.
  inline Object* GetProperty(String* key);
  inline Object* GetProperty(String* key, PropertyAttributes* attributes);
  Object* GetPropertyWithReceiver(Object* receiver,
                                  String* key,
                                  PropertyAttributes* attributes);
  Object* GetProperty(Object* receiver,
                      LookupResult* result,
                      String* key,
                      PropertyAttributes* attributes);
  Object* GetPropertyWithCallback(Object* receiver,
                                  Object* structure,
                                  String* name,
                                  Object* holder);
  Object* GetPropertyWithDefinedGetter(Object* receiver,
                                       JSFunction* getter);

  inline Object* GetElement(uint32_t index);
  Object* GetElementWithReceiver(Object* receiver, uint32_t index);

  // Return the object's prototype (might be Heap::null_value()).
  Object* GetPrototype();

  // Returns true if this is a JSValue containing a string and the index is
  // < the length of the string.  Used to implement [] on strings.
  inline bool IsStringObjectWithCharacterAt(uint32_t index);

#ifdef DEBUG
  // Prints this object with details.
  void Print();
  void PrintLn();
  // Verifies the object.
  void Verify();

  // Verify a pointer is a valid object pointer.
  static void VerifyPointer(Object* p);
#endif

  // Prints this object without details.
  void ShortPrint();

  // Prints this object without details to a message accumulator.
  void ShortPrint(StringStream* accumulator);

  // Casting: This cast is only needed to satisfy macros in objects-inl.h.
  static Object* cast(Object* value) { return value; }

  // Layout description.
  static const int kHeaderSize = 0;  // Object does not take up any space.

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Object);
};


// Smi represents integer Numbers that can be stored in 31 bits.
// Smis are immediate which means they are NOT allocated in the heap.
// The this pointer has the following format: [31 bit signed int] 0
// For long smis it has the following format:
//     [32 bit signed int] [31 bits zero padding] 0
// Smi stands for small integer.
class Smi: public Object {
 public:
  // Returns the integer value.
  inline int value();

  // Convert a value to a Smi object.
  static inline Smi* FromInt(int value);

  static inline Smi* FromIntptr(intptr_t value);

  // Returns whether value can be represented in a Smi.
  static inline bool IsValid(intptr_t value);

  // Casting.
  static inline Smi* cast(Object* object);

  // Dispatched behavior.
  void SmiPrint();
  void SmiPrint(StringStream* accumulator);
#ifdef DEBUG
  void SmiVerify();
#endif

  static const int kMinValue = (-1 << (kSmiValueSize - 1));
  static const int kMaxValue = -(kMinValue + 1);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Smi);
};


// Failure is used for reporting out of memory situations and
// propagating exceptions through the runtime system.  Failure objects
// are transient and cannot occur as part of the object graph.
//
// Failures are a single word, encoded as follows:
// +-------------------------+---+--+--+
// |...rrrrrrrrrrrrrrrrrrrrrr|sss|tt|11|
// +-------------------------+---+--+--+
//                          7 6 4 32 10
//
//
// The low two bits, 0-1, are the failure tag, 11.  The next two bits,
// 2-3, are a failure type tag 'tt' with possible values:
//   00 RETRY_AFTER_GC
//   01 EXCEPTION
//   10 INTERNAL_ERROR
//   11 OUT_OF_MEMORY_EXCEPTION
//
// The next three bits, 4-6, are an allocation space tag 'sss'.  The
// allocation space tag is 000 for all failure types except
// RETRY_AFTER_GC.  For RETRY_AFTER_GC, the possible values are the
// allocation spaces (the encoding is found in globals.h).
//
// The remaining bits is the size of the allocation request in units
// of the pointer size, and is zeroed except for RETRY_AFTER_GC
// failures.  The 25 bits (on a 32 bit platform) gives a representable
// range of 2^27 bytes (128MB).

// Failure type tag info.
const int kFailureTypeTagSize = 2;
const int kFailureTypeTagMask = (1 << kFailureTypeTagSize) - 1;

class Failure: public Object {
 public:
  // RuntimeStubs assumes EXCEPTION = 1 in the compiler-generated code.
  enum Type {
    RETRY_AFTER_GC = 0,
    EXCEPTION = 1,       // Returning this marker tells the real exception
                         // is in Top::pending_exception.
    INTERNAL_ERROR = 2,
    OUT_OF_MEMORY_EXCEPTION = 3
  };

  inline Type type() const;

  // Returns the space that needs to be collected for RetryAfterGC failures.
  inline AllocationSpace allocation_space() const;

  // Returns the number of bytes requested (up to the representable maximum)
  // for RetryAfterGC failures.
  inline int requested() const;

  inline bool IsInternalError() const;
  inline bool IsOutOfMemoryException() const;

  static Failure* RetryAfterGC(int requested_bytes, AllocationSpace space);
  static inline Failure* RetryAfterGC(int requested_bytes);  // NEW_SPACE
  static inline Failure* Exception();
  static inline Failure* InternalError();
  static inline Failure* OutOfMemoryException();
  // Casting.
  static inline Failure* cast(Object* object);

  // Dispatched behavior.
  void FailurePrint();
  void FailurePrint(StringStream* accumulator);
#ifdef DEBUG
  void FailureVerify();
#endif

 private:
  inline intptr_t value() const;
  static inline Failure* Construct(Type type, intptr_t value = 0);

  DISALLOW_IMPLICIT_CONSTRUCTORS(Failure);
};


// Heap objects typically have a map pointer in their first word.  However,
// during GC other data (eg, mark bits, forwarding addresses) is sometimes
// encoded in the first word.  The class MapWord is an abstraction of the
// value in a heap object's first word.
class MapWord BASE_EMBEDDED {
 public:
  // Normal state: the map word contains a map pointer.

  // Create a map word from a map pointer.
  static inline MapWord FromMap(Map* map);

  // View this map word as a map pointer.
  inline Map* ToMap();


  // Scavenge collection: the map word of live objects in the from space
  // contains a forwarding address (a heap object pointer in the to space).

  // True if this map word is a forwarding address for a scavenge
  // collection.  Only valid during a scavenge collection (specifically,
  // when all map words are heap object pointers, ie. not during a full GC).
  inline bool IsForwardingAddress();

  // Create a map word from a forwarding address.
  static inline MapWord FromForwardingAddress(HeapObject* object);

  // View this map word as a forwarding address.
  inline HeapObject* ToForwardingAddress();

  // True if this map word is a serialization address.  This will only be the
  // case during a destructive serialization of the heap.
  inline bool IsSerializationAddress();

  // Create a map word from a serialization address.
  static inline MapWord FromSerializationAddress(int raw);

  // View this map word as a serialization address.
  inline int ToSerializationAddress();

  // Marking phase of full collection: the map word of live objects is
  // marked, and may be marked as overflowed (eg, the object is live, its
  // children have not been visited, and it does not fit in the marking
  // stack).

  // True if this map word's mark bit is set.
  inline bool IsMarked();

  // Return this map word but with its mark bit set.
  inline void SetMark();

  // Return this map word but with its mark bit cleared.
  inline void ClearMark();

  // True if this map word's overflow bit is set.
  inline bool IsOverflowed();

  // Return this map word but with its overflow bit set.
  inline void SetOverflow();

  // Return this map word but with its overflow bit cleared.
  inline void ClearOverflow();


  // Compacting phase of a full compacting collection: the map word of live
  // objects contains an encoding of the original map address along with the
  // forwarding address (represented as an offset from the first live object
  // in the same page as the (old) object address).

  // Create a map word from a map address and a forwarding address offset.
  static inline MapWord EncodeAddress(Address map_address, int offset);

  // Return the map address encoded in this map word.
  inline Address DecodeMapAddress(MapSpace* map_space);

  // Return the forwarding offset encoded in this map word.
  inline int DecodeOffset();


  // During serialization: the map word is used to hold an encoded
  // address, and possibly a mark bit (set and cleared with SetMark
  // and ClearMark).

  // Create a map word from an encoded address.
  static inline MapWord FromEncodedAddress(Address address);

  inline Address ToEncodedAddress();

  // Bits used by the marking phase of the garbage collector.
  //
  // The first word of a heap object is normally a map pointer. The last two
  // bits are tagged as '01' (kHeapObjectTag). We reuse the last two bits to
  // mark an object as live and/or overflowed:
  //   last bit = 0, marked as alive
  //   second bit = 1, overflowed
  // An object is only marked as overflowed when it is marked as live while
  // the marking stack is overflowed.
  static const int kMarkingBit = 0;  // marking bit
  static const int kMarkingMask = (1 << kMarkingBit);  // marking mask
  static const int kOverflowBit = 1;  // overflow bit
  static const int kOverflowMask = (1 << kOverflowBit);  // overflow mask

  // Forwarding pointers and map pointer encoding
  //  31             21 20              10 9               0
  // +-----------------+------------------+-----------------+
  // |forwarding offset|page offset of map|page index of map|
  // +-----------------+------------------+-----------------+
  //  11 bits           11 bits            10 bits
  static const int kMapPageIndexBits = 10;
  static const int kMapPageOffsetBits = 11;
  static const int kForwardingOffsetBits = 11;

  static const int kMapPageIndexShift = 0;
  static const int kMapPageOffsetShift =
      kMapPageIndexShift + kMapPageIndexBits;
  static const int kForwardingOffsetShift =
      kMapPageOffsetShift + kMapPageOffsetBits;

  // 0x000003FF
  static const uint32_t kMapPageIndexMask =
      (1 << kMapPageOffsetShift) - 1;

  // 0x001FFC00
  static const uint32_t kMapPageOffsetMask =
      ((1 << kForwardingOffsetShift) - 1) & ~kMapPageIndexMask;

  // 0xFFE00000
  static const uint32_t kForwardingOffsetMask =
      ~(kMapPageIndexMask | kMapPageOffsetMask);

 private:
  // HeapObject calls the private constructor and directly reads the value.
  friend class HeapObject;

  explicit MapWord(uintptr_t value) : value_(value) {}

  uintptr_t value_;
};


// HeapObject is the superclass for all classes describing heap allocated
// objects.
class HeapObject: public Object {
 public:
  // [map]: Contains a map which contains the object's reflective
  // information.
  inline Map* map();
  inline void set_map(Map* value);

  // During garbage collection, the map word of a heap object does not
  // necessarily contain a map pointer.
  inline MapWord map_word();
  inline void set_map_word(MapWord map_word);

  // Converts an address to a HeapObject pointer.
  static inline HeapObject* FromAddress(Address address);

  // Returns the address of this HeapObject.
  inline Address address();

  // Iterates over pointers contained in the object (including the Map)
  void Iterate(ObjectVisitor* v);

  // Iterates over all pointers contained in the object except the
  // first map pointer.  The object type is given in the first
  // parameter. This function does not access the map pointer in the
  // object, and so is safe to call while the map pointer is modified.
  void IterateBody(InstanceType type, int object_size, ObjectVisitor* v);

  // This method only applies to struct objects.  Iterates over all the fields
  // of this struct.
  void IterateStructBody(int object_size, ObjectVisitor* v);

  // Returns the heap object's size in bytes
  inline int Size();

  // Given a heap object's map pointer, returns the heap size in bytes
  // Useful when the map pointer field is used for other purposes.
  // GC internal.
  inline int SizeFromMap(Map* map);

  // Support for the marking heap objects during the marking phase of GC.
  // True if the object is marked live.
  inline bool IsMarked();

  // Mutate this object's map pointer to indicate that the object is live.
  inline void SetMark();

  // Mutate this object's map pointer to remove the indication that the
  // object is live (ie, partially restore the map pointer).
  inline void ClearMark();

  // True if this object is marked as overflowed.  Overflowed objects have
  // been reached and marked during marking of the heap, but their children
  // have not necessarily been marked and they have not been pushed on the
  // marking stack.
  inline bool IsOverflowed();

  // Mutate this object's map pointer to indicate that the object is
  // overflowed.
  inline void SetOverflow();

  // Mutate this object's map pointer to remove the indication that the
  // object is overflowed (ie, partially restore the map pointer).
  inline void ClearOverflow();

  // Returns the field at offset in obj, as a read/write Object* reference.
  // Does no checking, and is safe to use during GC, while maps are invalid.
  // Does not update remembered sets, so should only be assigned to
  // during marking GC.
  static inline Object** RawField(HeapObject* obj, int offset);

  // Casting.
  static inline HeapObject* cast(Object* obj);

  // Return the write barrier mode for this.
  inline WriteBarrierMode GetWriteBarrierMode();

  // Dispatched behavior.
  void HeapObjectShortPrint(StringStream* accumulator);
#ifdef DEBUG
  void HeapObjectPrint();
  void HeapObjectVerify();
  inline void VerifyObjectField(int offset);

  void PrintHeader(const char* id);

  // Verify a pointer is a valid HeapObject pointer that points to object
  // areas in the heap.
  static void VerifyHeapPointer(Object* p);
#endif

  // Layout description.
  // First field in a heap object is map.
  static const int kMapOffset = Object::kHeaderSize;
  static const int kHeaderSize = kMapOffset + kPointerSize;

  STATIC_CHECK(kMapOffset == Internals::kHeapObjectMapOffset);

 protected:
  // helpers for calling an ObjectVisitor to iterate over pointers in the
  // half-open range [start, end) specified as integer offsets
  inline void IteratePointers(ObjectVisitor* v, int start, int end);
  // as above, for the single element at "offset"
  inline void IteratePointer(ObjectVisitor* v, int offset);

  // Computes the object size from the map.
  // Should only be used from SizeFromMap.
  int SlowSizeFromMap(Map* map);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(HeapObject);
};


// The HeapNumber class describes heap allocated numbers that cannot be
// represented in a Smi (small integer)
class HeapNumber: public HeapObject {
 public:
  // [value]: number value.
  inline double value();
  inline void set_value(double value);

  // Casting.
  static inline HeapNumber* cast(Object* obj);

  // Dispatched behavior.
  Object* HeapNumberToBoolean();
  void HeapNumberPrint();
  void HeapNumberPrint(StringStream* accumulator);
#ifdef DEBUG
  void HeapNumberVerify();
#endif

  // Layout description.
  static const int kValueOffset = HeapObject::kHeaderSize;
  // IEEE doubles are two 32 bit words.  The first is just mantissa, the second
  // is a mixture of sign, exponent and mantissa.  Our current platforms are all
  // little endian apart from non-EABI arm which is little endian with big
  // endian floating point word ordering!
#if !defined(V8_HOST_ARCH_ARM) || defined(USE_ARM_EABI)
  static const int kMantissaOffset = kValueOffset;
  static const int kExponentOffset = kValueOffset + 4;
#else
  static const int kMantissaOffset = kValueOffset + 4;
  static const int kExponentOffset = kValueOffset;
# define BIG_ENDIAN_FLOATING_POINT 1
#endif
  static const int kSize = kValueOffset + kDoubleSize;

  static const uint32_t kSignMask = 0x80000000u;
  static const uint32_t kExponentMask = 0x7ff00000u;
  static const uint32_t kMantissaMask = 0xfffffu;
  static const int kExponentBias = 1023;
  static const int kExponentShift = 20;
  static const int kMantissaBitsInTopWord = 20;
  static const int kNonMantissaBitsInTopWord = 12;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(HeapNumber);
};


// The JSObject describes real heap allocated JavaScript objects with
// properties.
// Note that the map of JSObject changes during execution to enable inline
// caching.
class JSObject: public HeapObject {
 public:
  enum DeleteMode { NORMAL_DELETION, FORCE_DELETION };
  enum ElementsKind {
    FAST_ELEMENTS,
    DICTIONARY_ELEMENTS,
    PIXEL_ELEMENTS,
    EXTERNAL_BYTE_ELEMENTS,
    EXTERNAL_UNSIGNED_BYTE_ELEMENTS,
    EXTERNAL_SHORT_ELEMENTS,
    EXTERNAL_UNSIGNED_SHORT_ELEMENTS,
    EXTERNAL_INT_ELEMENTS,
    EXTERNAL_UNSIGNED_INT_ELEMENTS,
    EXTERNAL_FLOAT_ELEMENTS
  };

  // [properties]: Backing storage for properties.
  // properties is a FixedArray in the fast case, and a Dictionary in the
  // slow case.
  DECL_ACCESSORS(properties, FixedArray)  // Get and set fast properties.
  inline void initialize_properties();
  inline bool HasFastProperties();
  inline StringDictionary* property_dictionary();  // Gets slow properties.

  // [elements]: The elements (properties with names that are integers).
  // elements is a FixedArray in the fast case, and a Dictionary in the slow
  // case or a PixelArray in a special case.
  DECL_ACCESSORS(elements, Array)  // Get and set fast elements.
  inline void initialize_elements();
  inline ElementsKind GetElementsKind();
  inline bool HasFastElements();
  inline bool HasDictionaryElements();
  inline bool HasPixelElements();
  inline bool HasExternalArrayElements();
  inline bool HasExternalByteElements();
  inline bool HasExternalUnsignedByteElements();
  inline bool HasExternalShortElements();
  inline bool HasExternalUnsignedShortElements();
  inline bool HasExternalIntElements();
  inline bool HasExternalUnsignedIntElements();
  inline bool HasExternalFloatElements();
  inline NumberDictionary* element_dictionary();  // Gets slow elements.

  // Collects elements starting at index 0.
  // Undefined values are placed after non-undefined values.
  // Returns the number of non-undefined values.
  Object* PrepareElementsForSort(uint32_t limit);
  // As PrepareElementsForSort, but only on objects where elements is
  // a dictionary, and it will stay a dictionary.
  Object* PrepareSlowElementsForSort(uint32_t limit);

  Object* SetProperty(String* key,
                      Object* value,
                      PropertyAttributes attributes);
  Object* SetProperty(LookupResult* result,
                      String* key,
                      Object* value,
                      PropertyAttributes attributes);
  Object* SetPropertyWithFailedAccessCheck(LookupResult* result,
                                           String* name,
                                           Object* value);
  Object* SetPropertyWithCallback(Object* structure,
                                  String* name,
                                  Object* value,
                                  JSObject* holder);
  Object* SetPropertyWithDefinedSetter(JSFunction* setter,
                                       Object* value);
  Object* SetPropertyWithInterceptor(String* name,
                                     Object* value,
                                     PropertyAttributes attributes);
  Object* SetPropertyPostInterceptor(String* name,
                                     Object* value,
                                     PropertyAttributes attributes);
  Object* IgnoreAttributesAndSetLocalProperty(String* key,
                                              Object* value,
                                              PropertyAttributes attributes);

  // Retrieve a value in a normalized object given a lookup result.
  // Handles the special representation of JS global objects.
  Object* GetNormalizedProperty(LookupResult* result);

  // Sets the property value in a normalized object given a lookup result.
  // Handles the special representation of JS global objects.
  Object* SetNormalizedProperty(LookupResult* result, Object* value);

  // Sets the property value in a normalized object given (key, value, details).
  // Handles the special representation of JS global objects.
  Object* SetNormalizedProperty(String* name,
                                Object* value,
                                PropertyDetails details);

  // Deletes the named property in a normalized object.
  Object* DeleteNormalizedProperty(String* name, DeleteMode mode);

  // Sets a property that currently has lazy loading.
  Object* SetLazyProperty(LookupResult* result,
                          String* name,
                          Object* value,
                          PropertyAttributes attributes);

  // Returns the class name ([[Class]] property in the specification).
  String* class_name();

  // Returns the constructor name (the name (possibly, inferred name) of the
  // function that was used to instantiate the object).
  String* constructor_name();

  // Retrieve interceptors.
  InterceptorInfo* GetNamedInterceptor();
  InterceptorInfo* GetIndexedInterceptor();

  inline PropertyAttributes GetPropertyAttribute(String* name);
  PropertyAttributes GetPropertyAttributeWithReceiver(JSObject* receiver,
                                                      String* name);
  PropertyAttributes GetLocalPropertyAttribute(String* name);

  Object* DefineAccessor(String* name, bool is_getter, JSFunction* fun,
                         PropertyAttributes attributes);
  Object* LookupAccessor(String* name, bool is_getter);

  // Used from Object::GetProperty().
  Object* GetPropertyWithFailedAccessCheck(Object* receiver,
                                           LookupResult* result,
                                           String* name,
                                           PropertyAttributes* attributes);
  Object* GetPropertyWithInterceptor(JSObject* receiver,
                                     String* name,
                                     PropertyAttributes* attributes);
  Object* GetPropertyPostInterceptor(JSObject* receiver,
                                     String* name,
                                     PropertyAttributes* attributes);
  Object* GetLocalPropertyPostInterceptor(JSObject* receiver,
                                          String* name,
                                          PropertyAttributes* attributes);
  Object* GetLazyProperty(Object* receiver,
                          LookupResult* result,
                          String* name,
                          PropertyAttributes* attributes);

  // Tells whether this object needs to be loaded.
  inline bool IsLoaded();

  // Returns true if this is an instance of an api function and has
  // been modified since it was created.  May give false positives.
  bool IsDirty();

  bool HasProperty(String* name) {
    return GetPropertyAttribute(name) != ABSENT;
  }

  // Can cause a GC if it hits an interceptor.
  bool HasLocalProperty(String* name) {
    return GetLocalPropertyAttribute(name) != ABSENT;
  }

  // If the receiver is a JSGlobalProxy this method will return its prototype,
  // otherwise the result is the receiver itself.
  inline Object* BypassGlobalProxy();

  // Accessors for hidden properties object.
  //
  // Hidden properties are not local properties of the object itself.
  // Instead they are stored on an auxiliary JSObject stored as a local
  // property with a special name Heap::hidden_symbol(). But if the
  // receiver is a JSGlobalProxy then the auxiliary object is a property
  // of its prototype.
  //
  // Has/Get/SetHiddenPropertiesObject methods don't allow the holder to be
  // a JSGlobalProxy. Use BypassGlobalProxy method above to get to the real
  // holder.
  //
  // These accessors do not touch interceptors or accessors.
  inline bool HasHiddenPropertiesObject();
  inline Object* GetHiddenPropertiesObject();
  inline Object* SetHiddenPropertiesObject(Object* hidden_obj);

  Object* DeleteProperty(String* name, DeleteMode mode);
  Object* DeleteElement(uint32_t index, DeleteMode mode);
  Object* DeleteLazyProperty(LookupResult* result,
                             String* name,
                             DeleteMode mode);

  // Tests for the fast common case for property enumeration.
  bool IsSimpleEnum();

  // Do we want to keep the elements in fast case when increasing the
  // capacity?
  bool ShouldConvertToSlowElements(int new_capacity);
  // Returns true if the backing storage for the slow-case elements of
  // this object takes up nearly as much space as a fast-case backing
  // storage would.  In that case the JSObject should have fast
  // elements.
  bool ShouldConvertToFastElements();

  // Return the object's prototype (might be Heap::null_value()).
  inline Object* GetPrototype();

  // Tells whether the index'th element is present.
  inline bool HasElement(uint32_t index);
  bool HasElementWithReceiver(JSObject* receiver, uint32_t index);
  bool HasLocalElement(uint32_t index);

  bool HasElementWithInterceptor(JSObject* receiver, uint32_t index);
  bool HasElementPostInterceptor(JSObject* receiver, uint32_t index);

  Object* SetFastElement(uint32_t index, Object* value);

  // Set the index'th array element.
  // A Failure object is returned if GC is needed.
  Object* SetElement(uint32_t index, Object* value);

  // Returns the index'th element.
  // The undefined object if index is out of bounds.
  Object* GetElementWithReceiver(JSObject* receiver, uint32_t index);

  void SetFastElements(FixedArray* elements);
  Object* SetSlowElements(Object* length);

  // Lookup interceptors are used for handling properties controlled by host
  // objects.
  inline bool HasNamedInterceptor();
  inline bool HasIndexedInterceptor();

  // Support functions for v8 api (needed for correct interceptor behavior).
  bool HasRealNamedProperty(String* key);
  bool HasRealElementProperty(uint32_t index);
  bool HasRealNamedCallbackProperty(String* key);

  // Initializes the array to a certain length
  Object* SetElementsLength(Object* length);

  // Get the header size for a JSObject.  Used to compute the index of
  // internal fields as well as the number of internal fields.
  inline int GetHeaderSize();

  inline int GetInternalFieldCount();
  inline Object* GetInternalField(int index);
  inline void SetInternalField(int index, Object* value);

  // Lookup a property.  If found, the result is valid and has
  // detailed information.
  void LocalLookup(String* name, LookupResult* result);
  void Lookup(String* name, LookupResult* result);

  // The following lookup functions skip interceptors.
  void LocalLookupRealNamedProperty(String* name, LookupResult* result);
  void LookupRealNamedProperty(String* name, LookupResult* result);
  void LookupRealNamedPropertyInPrototypes(String* name, LookupResult* result);
  void LookupCallbackSetterInPrototypes(String* name, LookupResult* result);
  Object* LookupCallbackSetterInPrototypes(uint32_t index);
  void LookupCallback(String* name, LookupResult* result);

  // Returns the number of properties on this object filtering out properties
  // with the specified attributes (ignoring interceptors).
  int NumberOfLocalProperties(PropertyAttributes filter);
  // Returns the number of enumerable properties (ignoring interceptors).
  int NumberOfEnumProperties();
  // Fill in details for properties into storage starting at the specified
  // index.
  void GetLocalPropertyNames(FixedArray* storage, int index);

  // Returns the number of properties on this object filtering out properties
  // with the specified attributes (ignoring interceptors).
  int NumberOfLocalElements(PropertyAttributes filter);
  // Returns the number of enumerable elements (ignoring interceptors).
  int NumberOfEnumElements();
  // Returns the number of elements on this object filtering out elements
  // with the specified attributes (ignoring interceptors).
  int GetLocalElementKeys(FixedArray* storage, PropertyAttributes filter);
  // Count and fill in the enumerable elements into storage.
  // (storage->length() == NumberOfEnumElements()).
  // If storage is NULL, will count the elements without adding
  // them to any storage.
  // Returns the number of enumerable elements.
  int GetEnumElementKeys(FixedArray* storage);

  // Add a property to a fast-case object using a map transition to
  // new_map.
  Object* AddFastPropertyUsingMap(Map* new_map,
                                  String* name,
                                  Object* value);

  // Add a constant function property to a fast-case object.
  // This leaves a CONSTANT_TRANSITION in the old map, and
  // if it is called on a second object with this map, a
  // normal property is added instead, with a map transition.
  // This avoids the creation of many maps with the same constant
  // function, all orphaned.
  Object* AddConstantFunctionProperty(String* name,
                                      JSFunction* function,
                                      PropertyAttributes attributes);

  Object* ReplaceSlowProperty(String* name,
                              Object* value,
                              PropertyAttributes attributes);

  // Converts a descriptor of any other type to a real field,
  // backed by the properties array.  Descriptors of visible
  // types, such as CONSTANT_FUNCTION, keep their enumeration order.
  // Converts the descriptor on the original object's map to a
  // map transition, and the the new field is on the object's new map.
  Object* ConvertDescriptorToFieldAndMapTransition(
      String* name,
      Object* new_value,
      PropertyAttributes attributes);

  // Converts a descriptor of any other type to a real field,
  // backed by the properties array.  Descriptors of visible
  // types, such as CONSTANT_FUNCTION, keep their enumeration order.
  Object* ConvertDescriptorToField(String* name,
                                   Object* new_value,
                                   PropertyAttributes attributes);

  // Add a property to a fast-case object.
  Object* AddFastProperty(String* name,
                          Object* value,
                          PropertyAttributes attributes);

  // Add a property to a slow-case object.
  Object* AddSlowProperty(String* name,
                          Object* value,
                          PropertyAttributes attributes);

  // Add a property to an object.
  Object* AddProperty(String* name,
                      Object* value,
                      PropertyAttributes attributes);

  // Convert the object to use the canonical dictionary
  // representation. If the object is expected to have additional properties
  // added this number can be indicated to have the backing store allocated to
  // an initial capacity for holding these properties.
  Object* NormalizeProperties(PropertyNormalizationMode mode,
                              int expected_additional_properties);
  Object* NormalizeElements();

  // Transform slow named properties to fast variants.
  // Returns failure if allocation failed.
  Object* TransformToFastProperties(int unused_property_fields);

  // Access fast-case object properties at index.
  inline Object* FastPropertyAt(int index);
  inline Object* FastPropertyAtPut(int index, Object* value);

  // Access to in object properties.
  inline Object* InObjectPropertyAt(int index);
  inline Object* InObjectPropertyAtPut(int index,
                                       Object* value,
                                       WriteBarrierMode mode
                                       = UPDATE_WRITE_BARRIER);

  // initializes the body after properties slot, properties slot is
  // initialized by set_properties
  // Note: this call does not update write barrier, it is caller's
  // reponsibility to ensure that *v* can be collected without WB here.
  inline void InitializeBody(int object_size);

  // Check whether this object references another object
  bool ReferencesObject(Object* obj);

  // Casting.
  static inline JSObject* cast(Object* obj);

  // Dispatched behavior.
  void JSObjectIterateBody(int object_size, ObjectVisitor* v);
  void JSObjectShortPrint(StringStream* accumulator);
#ifdef DEBUG
  void JSObjectPrint();
  void JSObjectVerify();
  void PrintProperties();
  void PrintElements();

  // Structure for collecting spill information about JSObjects.
  class SpillInformation {
   public:
    void Clear();
    void Print();
    int number_of_objects_;
    int number_of_objects_with_fast_properties_;
    int number_of_objects_with_fast_elements_;
    int number_of_fast_used_fields_;
    int number_of_fast_unused_fields_;
    int number_of_slow_used_properties_;
    int number_of_slow_unused_properties_;
    int number_of_fast_used_elements_;
    int number_of_fast_unused_elements_;
    int number_of_slow_used_elements_;
    int number_of_slow_unused_elements_;
  };

  void IncrementSpillStatistics(SpillInformation* info);
#endif
  Object* SlowReverseLookup(Object* value);

  static const uint32_t kMaxGap = 1024;
  static const int kMaxFastElementsLength = 5000;
  static const int kInitialMaxFastElementArray = 100000;
  static const int kMaxFastProperties = 8;
  static const int kMaxInstanceSize = 255 * kPointerSize;
  // When extending the backing storage for property values, we increase
  // its size by more than the 1 entry necessary, so sequentially adding fields
  // to the same object requires fewer allocations and copies.
  static const int kFieldsAdded = 3;

  // Layout description.
  static const int kPropertiesOffset = HeapObject::kHeaderSize;
  static const int kElementsOffset = kPropertiesOffset + kPointerSize;
  static const int kHeaderSize = kElementsOffset + kPointerSize;

  STATIC_CHECK(kHeaderSize == Internals::kJSObjectHeaderSize);

  Object* GetElementWithInterceptor(JSObject* receiver, uint32_t index);

 private:
  Object* SetElementWithInterceptor(uint32_t index, Object* value);
  Object* SetElementWithoutInterceptor(uint32_t index, Object* value);

  Object* GetElementPostInterceptor(JSObject* receiver, uint32_t index);

  Object* DeletePropertyPostInterceptor(String* name, DeleteMode mode);
  Object* DeletePropertyWithInterceptor(String* name);

  Object* DeleteElementPostInterceptor(uint32_t index, DeleteMode mode);
  Object* DeleteElementWithInterceptor(uint32_t index);

  PropertyAttributes GetPropertyAttributePostInterceptor(JSObject* receiver,
                                                         String* name,
                                                         bool continue_search);
  PropertyAttributes GetPropertyAttributeWithInterceptor(JSObject* receiver,
                                                         String* name,
                                                         bool continue_search);
  PropertyAttributes GetPropertyAttributeWithFailedAccessCheck(
      Object* receiver,
      LookupResult* result,
      String* name,
      bool continue_search);
  PropertyAttributes GetPropertyAttribute(JSObject* receiver,
                                          LookupResult* result,
                                          String* name,
                                          bool continue_search);

  // Returns true if most of the elements backing storage is used.
  bool HasDenseElements();

  Object* DefineGetterSetter(String* name, PropertyAttributes attributes);

  void LookupInDescriptor(String* name, LookupResult* result);

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSObject);
};


// Abstract super class arrays. It provides length behavior.
class Array: public HeapObject {
 public:
  // [length]: length of the array.
  inline int length();
  inline void set_length(int value);

  // Convert an object to an array index.
  // Returns true if the conversion succeeded.
  static inline bool IndexFromObject(Object* object, uint32_t* index);

  // Layout descriptor.
  static const int kLengthOffset = HeapObject::kHeaderSize;

 protected:
  // No code should use the Array class directly, only its subclasses.
  // Use the kHeaderSize of the appropriate subclass, which may be aligned.
  static const int kHeaderSize = kLengthOffset + kIntSize;
  static const int kAlignedSize = POINTER_SIZE_ALIGN(kHeaderSize);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Array);
};


// FixedArray describes fixed sized arrays where element
// type is Object*.

class FixedArray: public Array {
 public:

  // Setter and getter for elements.
  inline Object* get(int index);
  // Setter that uses write barrier.
  inline void set(int index, Object* value);

  // Setter that doesn't need write barrier).
  inline void set(int index, Smi* value);
  // Setter with explicit barrier mode.
  inline void set(int index, Object* value, WriteBarrierMode mode);

  // Setters for frequently used oddballs located in old space.
  inline void set_undefined(int index);
  inline void set_null(int index);
  inline void set_the_hole(int index);

  // Copy operations.
  inline Object* Copy();
  Object* CopySize(int new_length);

  // Add the elements of a JSArray to this FixedArray.
  Object* AddKeysFromJSArray(JSArray* array);

  // Compute the union of this and other.
  Object* UnionOfKeys(FixedArray* other);

  // Copy a sub array from the receiver to dest.
  void CopyTo(int pos, FixedArray* dest, int dest_pos, int len);

  // Garbage collection support.
  static int SizeFor(int length) { return kHeaderSize + length * kPointerSize; }

  // Code Generation support.
  static int OffsetOfElementAt(int index) { return SizeFor(index); }

  // Casting.
  static inline FixedArray* cast(Object* obj);

  // Align data at kPointerSize, even if Array.kHeaderSize isn't aligned.
  static const int kHeaderSize = POINTER_SIZE_ALIGN(Array::kHeaderSize);

  // Dispatched behavior.
  int FixedArraySize() { return SizeFor(length()); }
  void FixedArrayIterateBody(ObjectVisitor* v);
#ifdef DEBUG
  void FixedArrayPrint();
  void FixedArrayVerify();
  // Checks if two FixedArrays have identical contents.
  bool IsEqualTo(FixedArray* other);
#endif

  // Swap two elements in a pair of arrays.  If this array and the
  // numbers array are the same object, the elements are only swapped
  // once.
  void SwapPairs(FixedArray* numbers, int i, int j);

  // Sort prefix of this array and the numbers array as pairs wrt. the
  // numbers.  If the numbers array and the this array are the same
  // object, the prefix of this array is sorted.
  void SortPairs(FixedArray* numbers, uint32_t len);

 protected:
  // Set operation on FixedArray without using write barriers.
  static inline void fast_set(FixedArray* array, int index, Object* value);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedArray);
};


// DescriptorArrays are fixed arrays used to hold instance descriptors.
// The format of the these objects is:
//   [0]: point to a fixed array with (value, detail) pairs.
//   [1]: next enumeration index (Smi), or pointer to small fixed array:
//          [0]: next enumeration index (Smi)
//          [1]: pointer to fixed array with enum cache
//   [2]: first key
//   [length() - 1]: last key
//
class DescriptorArray: public FixedArray {
 public:
  // Is this the singleton empty_descriptor_array?
  inline bool IsEmpty();
  // Returns the number of descriptors in the array.
  int number_of_descriptors() {
    return IsEmpty() ? 0 : length() - kFirstIndex;
  }

  int NextEnumerationIndex() {
    if (IsEmpty()) return PropertyDetails::kInitialIndex;
    Object* obj = get(kEnumerationIndexIndex);
    if (obj->IsSmi()) {
      return Smi::cast(obj)->value();
    } else {
      Object* index = FixedArray::cast(obj)->get(kEnumCacheBridgeEnumIndex);
      return Smi::cast(index)->value();
    }
  }

  // Set next enumeration index and flush any enum cache.
  void SetNextEnumerationIndex(int value) {
    if (!IsEmpty()) {
      fast_set(this, kEnumerationIndexIndex, Smi::FromInt(value));
    }
  }
  bool HasEnumCache() {
    return !IsEmpty() && !get(kEnumerationIndexIndex)->IsSmi();
  }

  Object* GetEnumCache() {
    ASSERT(HasEnumCache());
    FixedArray* bridge = FixedArray::cast(get(kEnumerationIndexIndex));
    return bridge->get(kEnumCacheBridgeCacheIndex);
  }

  // Initialize or change the enum cache,
  // using the supplied storage for the small "bridge".
  void SetEnumCache(FixedArray* bridge_storage, FixedArray* new_cache);

  // Accessors for fetching instance descriptor at descriptor number.
  inline String* GetKey(int descriptor_number);
  inline Object* GetValue(int descriptor_number);
  inline Smi* GetDetails(int descriptor_number);
  inline PropertyType GetType(int descriptor_number);
  inline int GetFieldIndex(int descriptor_number);
  inline JSFunction* GetConstantFunction(int descriptor_number);
  inline Object* GetCallbacksObject(int descriptor_number);
  inline AccessorDescriptor* GetCallbacks(int descriptor_number);
  inline bool IsProperty(int descriptor_number);
  inline bool IsTransition(int descriptor_number);
  inline bool IsNullDescriptor(int descriptor_number);
  inline bool IsDontEnum(int descriptor_number);

  // Accessor for complete descriptor.
  inline void Get(int descriptor_number, Descriptor* desc);
  inline void Set(int descriptor_number, Descriptor* desc);

  // Transfer complete descriptor from another descriptor array to
  // this one.
  inline void CopyFrom(int index, DescriptorArray* src, int src_index);

  // Copy the descriptor array, insert a new descriptor and optionally
  // remove map transitions.  If the descriptor is already present, it is
  // replaced.  If a replaced descriptor is a real property (not a transition
  // or null), its enumeration index is kept as is.
  // If adding a real property, map transitions must be removed.  If adding
  // a transition, they must not be removed.  All null descriptors are removed.
  Object* CopyInsert(Descriptor* descriptor, TransitionFlag transition_flag);

  // Remove all transitions.  Return  a copy of the array with all transitions
  // removed, or a Failure object if the new array could not be allocated.
  Object* RemoveTransitions();

  // Sort the instance descriptors by the hash codes of their keys.
  void Sort();

  // Search the instance descriptors for given name.
  inline int Search(String* name);

  // Tells whether the name is present int the array.
  bool Contains(String* name) { return kNotFound != Search(name); }

  // Perform a binary search in the instance descriptors represented
  // by this fixed array.  low and high are descriptor indices.  If there
  // are three instance descriptors in this array it should be called
  // with low=0 and high=2.
  int BinarySearch(String* name, int low, int high);

  // Perform a linear search in the instance descriptors represented
  // by this fixed array.  len is the number of descriptor indices that are
  // valid.  Does not require the descriptors to be sorted.
  int LinearSearch(String* name, int len);

  // Allocates a DescriptorArray, but returns the singleton
  // empty descriptor array object if number_of_descriptors is 0.
  static Object* Allocate(int number_of_descriptors);

  // Casting.
  static inline DescriptorArray* cast(Object* obj);

  // Constant for denoting key was not found.
  static const int kNotFound = -1;

  static const int kContentArrayIndex = 0;
  static const int kEnumerationIndexIndex = 1;
  static const int kFirstIndex = 2;

  // The length of the "bridge" to the enum cache.
  static const int kEnumCacheBridgeLength = 2;
  static const int kEnumCacheBridgeEnumIndex = 0;
  static const int kEnumCacheBridgeCacheIndex = 1;

  // Layout description.
  static const int kContentArrayOffset = FixedArray::kHeaderSize;
  static const int kEnumerationIndexOffset = kContentArrayOffset + kPointerSize;
  static const int kFirstOffset = kEnumerationIndexOffset + kPointerSize;

  // Layout description for the bridge array.
  static const int kEnumCacheBridgeEnumOffset = FixedArray::kHeaderSize;
  static const int kEnumCacheBridgeCacheOffset =
    kEnumCacheBridgeEnumOffset + kPointerSize;

#ifdef DEBUG
  // Print all the descriptors.
  void PrintDescriptors();

  // Is the descriptor array sorted and without duplicates?
  bool IsSortedNoDuplicates();

  // Are two DescriptorArrays equal?
  bool IsEqualTo(DescriptorArray* other);
#endif

  // The maximum number of descriptors we want in a descriptor array (should
  // fit in a page).
  static const int kMaxNumberOfDescriptors = 1024 + 512;

 private:
  // Conversion from descriptor number to array indices.
  static int ToKeyIndex(int descriptor_number) {
    return descriptor_number+kFirstIndex;
  }
  static int ToValueIndex(int descriptor_number) {
    return descriptor_number << 1;
  }
  static int ToDetailsIndex(int descriptor_number) {
    return( descriptor_number << 1) + 1;
  }

  bool is_null_descriptor(int descriptor_number) {
    return PropertyDetails(GetDetails(descriptor_number)).type() ==
        NULL_DESCRIPTOR;
  }
  // Swap operation on FixedArray without using write barriers.
  static inline void fast_swap(FixedArray* array, int first, int second);

  // Swap descriptor first and second.
  inline void Swap(int first, int second);

  FixedArray* GetContentArray() {
    return FixedArray::cast(get(kContentArrayIndex));
  }
  DISALLOW_IMPLICIT_CONSTRUCTORS(DescriptorArray);
};


// HashTable is a subclass of FixedArray that implements a hash table
// that uses open addressing and quadratic probing.
//
// In order for the quadratic probing to work, elements that have not
// yet been used and elements that have been deleted are
// distinguished.  Probing continues when deleted elements are
// encountered and stops when unused elements are encountered.
//
// - Elements with key == undefined have not been used yet.
// - Elements with key == null have been deleted.
//
// The hash table class is parameterized with a Shape and a Key.
// Shape must be a class with the following interface:
//   class ExampleShape {
//    public:
//      // Tells whether key matches other.
//     static bool IsMatch(Key key, Object* other);
//     // Returns the hash value for key.
//     static uint32_t Hash(Key key);
//     // Returns the hash value for object.
//     static uint32_t HashForObject(Key key, Object* object);
//     // Convert key to an object.
//     static inline Object* AsObject(Key key);
//     // The prefix size indicates number of elements in the beginning
//     // of the backing storage.
//     static const int kPrefixSize = ..;
//     // The Element size indicates number of elements per entry.
//     static const int kEntrySize = ..;
//   };
// The prefix size indicates an amount of memory in the
// beginning of the backing storage that can be used for non-element
// information by subclasses.

template<typename Shape, typename Key>
class HashTable: public FixedArray {
 public:
  // Returns the number of elements in the hash table.
  int NumberOfElements() {
    return Smi::cast(get(kNumberOfElementsIndex))->value();
  }

  // Returns the capacity of the hash table.
  int Capacity() {
    return Smi::cast(get(kCapacityIndex))->value();
  }

  // ElementAdded should be called whenever an element is added to a
  // hash table.
  void ElementAdded() { SetNumberOfElements(NumberOfElements() + 1); }

  // ElementRemoved should be called whenever an element is removed from
  // a hash table.
  void ElementRemoved() { SetNumberOfElements(NumberOfElements() - 1); }
  void ElementsRemoved(int n) { SetNumberOfElements(NumberOfElements() - n); }

  // Returns a new HashTable object. Might return Failure.
  static Object* Allocate(int at_least_space_for);

  // Returns the key at entry.
  Object* KeyAt(int entry) { return get(EntryToIndex(entry)); }

  // Tells whether k is a real key.  Null and undefined are not allowed
  // as keys and can be used to indicate missing or deleted elements.
  bool IsKey(Object* k) {
    return !k->IsNull() && !k->IsUndefined();
  }

  // Garbage collection support.
  void IteratePrefix(ObjectVisitor* visitor);
  void IterateElements(ObjectVisitor* visitor);

  // Casting.
  static inline HashTable* cast(Object* obj);

  // Compute the probe offset (quadratic probing).
  INLINE(static uint32_t GetProbeOffset(uint32_t n)) {
    return (n + n * n) >> 1;
  }

  static const int kNumberOfElementsIndex = 0;
  static const int kCapacityIndex         = 1;
  static const int kPrefixStartIndex      = 2;
  static const int kElementsStartIndex    =
      kPrefixStartIndex + Shape::kPrefixSize;
  static const int kEntrySize             = Shape::kEntrySize;
  static const int kElementsStartOffset   =
      kHeaderSize + kElementsStartIndex * kPointerSize;

  // Constant used for denoting a absent entry.
  static const int kNotFound = -1;

  // Find entry for key otherwise return -1.
  int FindEntry(Key key);

 protected:

  // Find the entry at which to insert element with the given key that
  // has the given hash value.
  uint32_t FindInsertionEntry(uint32_t hash);

  // Returns the index for an entry (of the key)
  static inline int EntryToIndex(int entry) {
    return (entry * kEntrySize) + kElementsStartIndex;
  }

  // Update the number of elements in the hash table.
  void SetNumberOfElements(int nof) {
    fast_set(this, kNumberOfElementsIndex, Smi::FromInt(nof));
  }

  // Sets the capacity of the hash table.
  void SetCapacity(int capacity) {
    // To scale a computed hash code to fit within the hash table, we
    // use bit-wise AND with a mask, so the capacity must be positive
    // and non-zero.
    ASSERT(capacity > 0);
    fast_set(this, kCapacityIndex, Smi::FromInt(capacity));
  }


  // Returns probe entry.
  static uint32_t GetProbe(uint32_t hash, uint32_t number, uint32_t size) {
    ASSERT(IsPowerOf2(size));
    return (hash + GetProbeOffset(number)) & (size - 1);
  }

  // Ensure enough space for n additional elements.
  Object* EnsureCapacity(int n, Key key);
};



// HashTableKey is an abstract superclass for virtual key behavior.
class HashTableKey {
 public:
  // Returns whether the other object matches this key.
  virtual bool IsMatch(Object* other) = 0;
  // Returns the hash value for this key.
  virtual uint32_t Hash() = 0;
  // Returns the hash value for object.
  virtual uint32_t HashForObject(Object* key) = 0;
  // Returns the key object for storing into the hash table.
  // If allocations fails a failure object is returned.
  virtual Object* AsObject() = 0;
  // Required.
  virtual ~HashTableKey() {}
};

class SymbolTableShape {
 public:
  static bool IsMatch(HashTableKey* key, Object* value) {
    return key->IsMatch(value);
  }
  static uint32_t Hash(HashTableKey* key) {
    return key->Hash();
  }
  static uint32_t HashForObject(HashTableKey* key, Object* object) {
    return key->HashForObject(object);
  }
  static Object* AsObject(HashTableKey* key) {
    return key->AsObject();
  }

  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

// SymbolTable.
//
// No special elements in the prefix and the element size is 1
// because only the symbol itself (the key) needs to be stored.
class SymbolTable: public HashTable<SymbolTableShape, HashTableKey*> {
 public:
  // Find symbol in the symbol table.  If it is not there yet, it is
  // added.  The return value is the symbol table which might have
  // been enlarged.  If the return value is not a failure, the symbol
  // pointer *s is set to the symbol found.
  Object* LookupSymbol(Vector<const char> str, Object** s);
  Object* LookupString(String* key, Object** s);

  // Looks up a symbol that is equal to the given string and returns
  // true if it is found, assigning the symbol to the given output
  // parameter.
  bool LookupSymbolIfExists(String* str, String** symbol);
  bool LookupTwoCharsSymbolIfExists(uint32_t c1, uint32_t c2, String** symbol);

  // Casting.
  static inline SymbolTable* cast(Object* obj);

 private:
  Object* LookupKey(HashTableKey* key, Object** s);

  DISALLOW_IMPLICIT_CONSTRUCTORS(SymbolTable);
};


class MapCacheShape {
 public:
  static bool IsMatch(HashTableKey* key, Object* value) {
    return key->IsMatch(value);
  }
  static uint32_t Hash(HashTableKey* key) {
    return key->Hash();
  }

  static uint32_t HashForObject(HashTableKey* key, Object* object) {
    return key->HashForObject(object);
  }

  static Object* AsObject(HashTableKey* key) {
    return key->AsObject();
  }

  static const int kPrefixSize = 0;
  static const int kEntrySize = 2;
};


// MapCache.
//
// Maps keys that are a fixed array of symbols to a map.
// Used for canonicalize maps for object literals.
class MapCache: public HashTable<MapCacheShape, HashTableKey*> {
 public:
  // Find cached value for a string key, otherwise return null.
  Object* Lookup(FixedArray* key);
  Object* Put(FixedArray* key, Map* value);
  static inline MapCache* cast(Object* obj);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MapCache);
};


template <typename Shape, typename Key>
class Dictionary: public HashTable<Shape, Key> {
 public:

  static inline Dictionary<Shape, Key>* cast(Object* obj) {
    return reinterpret_cast<Dictionary<Shape, Key>*>(obj);
  }

  // Returns the value at entry.
  Object* ValueAt(int entry) {
    return get(HashTable<Shape, Key>::EntryToIndex(entry)+1);
  }

  // Set the value for entry.
  void ValueAtPut(int entry, Object* value) {
    set(HashTable<Shape, Key>::EntryToIndex(entry)+1, value);
  }

  // Returns the property details for the property at entry.
  PropertyDetails DetailsAt(int entry) {
    ASSERT(entry >= 0);  // Not found is -1, which is not caught by get().
    return PropertyDetails(
        Smi::cast(get(HashTable<Shape, Key>::EntryToIndex(entry) + 2)));
  }

  // Set the details for entry.
  void DetailsAtPut(int entry, PropertyDetails value) {
    set(HashTable<Shape, Key>::EntryToIndex(entry) + 2, value.AsSmi());
  }

  // Sorting support
  void CopyValuesTo(FixedArray* elements);

  // Delete a property from the dictionary.
  Object* DeleteProperty(int entry, JSObject::DeleteMode mode);

  // Returns the number of elements in the dictionary filtering out properties
  // with the specified attributes.
  int NumberOfElementsFilterAttributes(PropertyAttributes filter);

  // Returns the number of enumerable elements in the dictionary.
  int NumberOfEnumElements();

  // Copies keys to preallocated fixed array.
  void CopyKeysTo(FixedArray* storage, PropertyAttributes filter);
  // Fill in details for properties into storage.
  void CopyKeysTo(FixedArray* storage);

  // Accessors for next enumeration index.
  void SetNextEnumerationIndex(int index) {
    fast_set(this, kNextEnumerationIndexIndex, Smi::FromInt(index));
  }

  int NextEnumerationIndex() {
    return Smi::cast(FixedArray::get(kNextEnumerationIndexIndex))->value();
  }

  // Returns a new array for dictionary usage. Might return Failure.
  static Object* Allocate(int at_least_space_for);

  // Ensure enough space for n additional elements.
  Object* EnsureCapacity(int n, Key key);

#ifdef DEBUG
  void Print();
#endif
  // Returns the key (slow).
  Object* SlowReverseLookup(Object* value);

  // Sets the entry to (key, value) pair.
  inline void SetEntry(int entry,
                       Object* key,
                       Object* value,
                       PropertyDetails details);

  Object* Add(Key key, Object* value, PropertyDetails details);

 protected:
  // Generic at put operation.
  Object* AtPut(Key key, Object* value);

  // Add entry to dictionary.
  Object* AddEntry(Key key,
                   Object* value,
                   PropertyDetails details,
                   uint32_t hash);

  // Generate new enumeration indices to avoid enumeration index overflow.
  Object* GenerateNewEnumerationIndices();
  static const int kMaxNumberKeyIndex =
      HashTable<Shape, Key>::kPrefixStartIndex;
  static const int kNextEnumerationIndexIndex = kMaxNumberKeyIndex + 1;
};


class StringDictionaryShape {
 public:
  static inline bool IsMatch(String* key, Object* other);
  static inline uint32_t Hash(String* key);
  static inline uint32_t HashForObject(String* key, Object* object);
  static inline Object* AsObject(String* key);
  static const int kPrefixSize = 2;
  static const int kEntrySize = 3;
  static const bool kIsEnumerable = true;
};


class StringDictionary: public Dictionary<StringDictionaryShape, String*> {
 public:
  static inline StringDictionary* cast(Object* obj) {
    ASSERT(obj->IsDictionary());
    return reinterpret_cast<StringDictionary*>(obj);
  }

  // Copies enumerable keys to preallocated fixed array.
  void CopyEnumKeysTo(FixedArray* storage, FixedArray* sort_array);

  // For transforming properties of a JSObject.
  Object* TransformPropertiesToFastFor(JSObject* obj,
                                       int unused_property_fields);
};


class NumberDictionaryShape {
 public:
  static inline bool IsMatch(uint32_t key, Object* other);
  static inline uint32_t Hash(uint32_t key);
  static inline uint32_t HashForObject(uint32_t key, Object* object);
  static inline Object* AsObject(uint32_t key);
  static const int kPrefixSize = 2;
  static const int kEntrySize = 3;
  static const bool kIsEnumerable = false;
};


class NumberDictionary: public Dictionary<NumberDictionaryShape, uint32_t> {
 public:
  static NumberDictionary* cast(Object* obj) {
    ASSERT(obj->IsDictionary());
    return reinterpret_cast<NumberDictionary*>(obj);
  }

  // Type specific at put (default NONE attributes is used when adding).
  Object* AtNumberPut(uint32_t key, Object* value);
  Object* AddNumberEntry(uint32_t key,
                         Object* value,
                         PropertyDetails details);

  // Set an existing entry or add a new one if needed.
  Object* Set(uint32_t key, Object* value, PropertyDetails details);

  void UpdateMaxNumberKey(uint32_t key);

  // If slow elements are required we will never go back to fast-case
  // for the elements kept in this dictionary.  We require slow
  // elements if an element has been added at an index larger than
  // kRequiresSlowElementsLimit or set_requires_slow_elements() has been called
  // when defining a getter or setter with a number key.
  inline bool requires_slow_elements();
  inline void set_requires_slow_elements();

  // Get the value of the max number key that has been added to this
  // dictionary.  max_number_key can only be called if
  // requires_slow_elements returns false.
  inline uint32_t max_number_key();

  // Remove all entries were key is a number and (from <= key && key < to).
  void RemoveNumberEntries(uint32_t from, uint32_t to);

  // Bit masks.
  static const int kRequiresSlowElementsMask = 1;
  static const int kRequiresSlowElementsTagSize = 1;
  static const uint32_t kRequiresSlowElementsLimit = (1 << 29) - 1;
};


// ByteArray represents fixed sized byte arrays.  Used by the outside world,
// such as PCRE, and also by the memory allocator and garbage collector to
// fill in free blocks in the heap.
class ByteArray: public Array {
 public:
  // Setter and getter.
  inline byte get(int index);
  inline void set(int index, byte value);

  // Treat contents as an int array.
  inline int get_int(int index);

  static int SizeFor(int length) {
    return OBJECT_SIZE_ALIGN(kHeaderSize + length);
  }
  // We use byte arrays for free blocks in the heap.  Given a desired size in
  // bytes that is a multiple of the word size and big enough to hold a byte
  // array, this function returns the number of elements a byte array should
  // have.
  static int LengthFor(int size_in_bytes) {
    ASSERT(IsAligned(size_in_bytes, kPointerSize));
    ASSERT(size_in_bytes >= kHeaderSize);
    return size_in_bytes - kHeaderSize;
  }

  // Returns data start address.
  inline Address GetDataStartAddress();

  // Returns a pointer to the ByteArray object for a given data start address.
  static inline ByteArray* FromDataStartAddress(Address address);

  // Casting.
  static inline ByteArray* cast(Object* obj);

  // Dispatched behavior.
  int ByteArraySize() { return SizeFor(length()); }
#ifdef DEBUG
  void ByteArrayPrint();
  void ByteArrayVerify();
#endif

  // ByteArray headers are not quadword aligned.
  static const int kHeaderSize = Array::kHeaderSize;
  static const int kAlignedSize = Array::kAlignedSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ByteArray);
};


// A PixelArray represents a fixed-size byte array with special semantics
// used for implementing the CanvasPixelArray object. Please see the
// specification at:
// http://www.whatwg.org/specs/web-apps/current-work/
//                      multipage/the-canvas-element.html#canvaspixelarray
// In particular, write access clamps the value written to 0 or 255 if the
// value written is outside this range.
class PixelArray: public Array {
 public:
  // [external_pointer]: The pointer to the external memory area backing this
  // pixel array.
  DECL_ACCESSORS(external_pointer, uint8_t)  // Pointer to the data store.

  // Setter and getter.
  inline uint8_t get(int index);
  inline void set(int index, uint8_t value);

  // This accessor applies the correct conversion from Smi, HeapNumber and
  // undefined and clamps the converted value between 0 and 255.
  Object* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline PixelArray* cast(Object* obj);

#ifdef DEBUG
  void PixelArrayPrint();
  void PixelArrayVerify();
#endif  // DEBUG

  // Maximal acceptable length for a pixel array.
  static const int kMaxLength = 0x3fffffff;

  // PixelArray headers are not quadword aligned.
  static const int kExternalPointerOffset = Array::kAlignedSize;
  static const int kHeaderSize = kExternalPointerOffset + kPointerSize;
  static const int kAlignedSize = OBJECT_SIZE_ALIGN(kHeaderSize);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PixelArray);
};


// An ExternalArray represents a fixed-size array of primitive values
// which live outside the JavaScript heap. Its subclasses are used to
// implement the CanvasArray types being defined in the WebGL
// specification. As of this writing the first public draft is not yet
// available, but Khronos members can access the draft at:
//   https://cvs.khronos.org/svn/repos/3dweb/trunk/doc/spec/WebGL-spec.html
//
// The semantics of these arrays differ from CanvasPixelArray.
// Out-of-range values passed to the setter are converted via a C
// cast, not clamping. Out-of-range indices cause exceptions to be
// raised rather than being silently ignored.
class ExternalArray: public Array {
 public:
  // [external_pointer]: The pointer to the external memory area backing this
  // external array.
  DECL_ACCESSORS(external_pointer, void)  // Pointer to the data store.

  // Casting.
  static inline ExternalArray* cast(Object* obj);

  // Maximal acceptable length for an external array.
  static const int kMaxLength = 0x3fffffff;

  // ExternalArray headers are not quadword aligned.
  static const int kExternalPointerOffset = Array::kAlignedSize;
  static const int kHeaderSize = kExternalPointerOffset + kPointerSize;
  static const int kAlignedSize = OBJECT_SIZE_ALIGN(kHeaderSize);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalArray);
};


class ExternalByteArray: public ExternalArray {
 public:
  // Setter and getter.
  inline int8_t get(int index);
  inline void set(int index, int8_t value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  Object* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalByteArray* cast(Object* obj);

#ifdef DEBUG
  void ExternalByteArrayPrint();
  void ExternalByteArrayVerify();
#endif  // DEBUG

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalByteArray);
};


class ExternalUnsignedByteArray: public ExternalArray {
 public:
  // Setter and getter.
  inline uint8_t get(int index);
  inline void set(int index, uint8_t value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  Object* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalUnsignedByteArray* cast(Object* obj);

#ifdef DEBUG
  void ExternalUnsignedByteArrayPrint();
  void ExternalUnsignedByteArrayVerify();
#endif  // DEBUG

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalUnsignedByteArray);
};


class ExternalShortArray: public ExternalArray {
 public:
  // Setter and getter.
  inline int16_t get(int index);
  inline void set(int index, int16_t value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  Object* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalShortArray* cast(Object* obj);

#ifdef DEBUG
  void ExternalShortArrayPrint();
  void ExternalShortArrayVerify();
#endif  // DEBUG

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalShortArray);
};


class ExternalUnsignedShortArray: public ExternalArray {
 public:
  // Setter and getter.
  inline uint16_t get(int index);
  inline void set(int index, uint16_t value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  Object* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalUnsignedShortArray* cast(Object* obj);

#ifdef DEBUG
  void ExternalUnsignedShortArrayPrint();
  void ExternalUnsignedShortArrayVerify();
#endif  // DEBUG

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalUnsignedShortArray);
};


class ExternalIntArray: public ExternalArray {
 public:
  // Setter and getter.
  inline int32_t get(int index);
  inline void set(int index, int32_t value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  Object* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalIntArray* cast(Object* obj);

#ifdef DEBUG
  void ExternalIntArrayPrint();
  void ExternalIntArrayVerify();
#endif  // DEBUG

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalIntArray);
};


class ExternalUnsignedIntArray: public ExternalArray {
 public:
  // Setter and getter.
  inline uint32_t get(int index);
  inline void set(int index, uint32_t value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  Object* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalUnsignedIntArray* cast(Object* obj);

#ifdef DEBUG
  void ExternalUnsignedIntArrayPrint();
  void ExternalUnsignedIntArrayVerify();
#endif  // DEBUG

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalUnsignedIntArray);
};


class ExternalFloatArray: public ExternalArray {
 public:
  // Setter and getter.
  inline float get(int index);
  inline void set(int index, float value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  Object* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalFloatArray* cast(Object* obj);

#ifdef DEBUG
  void ExternalFloatArrayPrint();
  void ExternalFloatArrayVerify();
#endif  // DEBUG

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalFloatArray);
};


// Code describes objects with on-the-fly generated machine code.
class Code: public HeapObject {
 public:
  // Opaque data type for encapsulating code flags like kind, inline
  // cache state, and arguments count.
  enum Flags { };

  enum Kind {
    FUNCTION,
    STUB,
    BUILTIN,
    LOAD_IC,
    KEYED_LOAD_IC,
    CALL_IC,
    STORE_IC,
    KEYED_STORE_IC,
    // No more than eight kinds. The value currently encoded in three bits in
    // Flags.

    // Pseudo-kinds.
    REGEXP = BUILTIN,
    FIRST_IC_KIND = LOAD_IC,
    LAST_IC_KIND = KEYED_STORE_IC
  };

  enum {
    NUMBER_OF_KINDS = KEYED_STORE_IC + 1
  };

#ifdef ENABLE_DISASSEMBLER
  // Printing
  static const char* Kind2String(Kind kind);
  static const char* ICState2String(InlineCacheState state);
  static const char* PropertyType2String(PropertyType type);
  void Disassemble(const char* name);
#endif  // ENABLE_DISASSEMBLER

  // [instruction_size]: Size of the native instructions
  inline int instruction_size();
  inline void set_instruction_size(int value);

  // [relocation_size]: Size of relocation information.
  inline int relocation_size();
  inline void set_relocation_size(int value);

  // [sinfo_size]: Size of scope information.
  inline int sinfo_size();
  inline void set_sinfo_size(int value);

  // [flags]: Various code flags.
  inline Flags flags();
  inline void set_flags(Flags flags);

  // [flags]: Access to specific code flags.
  inline Kind kind();
  inline InlineCacheState ic_state();  // Only valid for IC stubs.
  inline InLoopFlag ic_in_loop();  // Only valid for IC stubs.
  inline PropertyType type();  // Only valid for monomorphic IC stubs.
  inline int arguments_count();  // Only valid for call IC stubs.

  // Testers for IC stub kinds.
  inline bool is_inline_cache_stub();
  inline bool is_load_stub() { return kind() == LOAD_IC; }
  inline bool is_keyed_load_stub() { return kind() == KEYED_LOAD_IC; }
  inline bool is_store_stub() { return kind() == STORE_IC; }
  inline bool is_keyed_store_stub() { return kind() == KEYED_STORE_IC; }
  inline bool is_call_stub() { return kind() == CALL_IC; }

  // [major_key]: For kind STUB, the major key.
  inline CodeStub::Major major_key();
  inline void set_major_key(CodeStub::Major major);

  // Flags operations.
  static inline Flags ComputeFlags(Kind kind,
                                   InLoopFlag in_loop = NOT_IN_LOOP,
                                   InlineCacheState ic_state = UNINITIALIZED,
                                   PropertyType type = NORMAL,
                                   int argc = -1);

  static inline Flags ComputeMonomorphicFlags(
      Kind kind,
      PropertyType type,
      InLoopFlag in_loop = NOT_IN_LOOP,
      int argc = -1);

  static inline Kind ExtractKindFromFlags(Flags flags);
  static inline InlineCacheState ExtractICStateFromFlags(Flags flags);
  static inline InLoopFlag ExtractICInLoopFromFlags(Flags flags);
  static inline PropertyType ExtractTypeFromFlags(Flags flags);
  static inline int ExtractArgumentsCountFromFlags(Flags flags);
  static inline Flags RemoveTypeFromFlags(Flags flags);

  // Convert a target address into a code object.
  static inline Code* GetCodeFromTargetAddress(Address address);

  // Returns the address of the first instruction.
  inline byte* instruction_start();

  // Returns the size of the instructions, padding, and relocation information.
  inline int body_size();

  // Returns the address of the first relocation info (read backwards!).
  inline byte* relocation_start();

  // Code entry point.
  inline byte* entry();

  // Returns true if pc is inside this object's instructions.
  inline bool contains(byte* pc);

  // Returns the address of the scope information.
  inline byte* sinfo_start();

  // Relocate the code by delta bytes. Called to signal that this code
  // object has been moved by delta bytes.
  void Relocate(intptr_t delta);

  // Migrate code described by desc.
  void CopyFrom(const CodeDesc& desc);

  // Returns the object size for a given body and sinfo size (Used for
  // allocation).
  static int SizeFor(int body_size, int sinfo_size) {
    ASSERT_SIZE_TAG_ALIGNED(body_size);
    ASSERT_SIZE_TAG_ALIGNED(sinfo_size);
    return RoundUp(kHeaderSize + body_size + sinfo_size, kCodeAlignment);
  }

  // Calculate the size of the code object to report for log events. This takes
  // the layout of the code object into account.
  int ExecutableSize() {
    // Check that the assumptions about the layout of the code object holds.
    ASSERT_EQ(static_cast<int>(instruction_start() - address()),
              Code::kHeaderSize);
    return instruction_size() + Code::kHeaderSize;
  }

  // Locating source position.
  int SourcePosition(Address pc);
  int SourceStatementPosition(Address pc);

  // Casting.
  static inline Code* cast(Object* obj);

  // Dispatched behavior.
  int CodeSize() { return SizeFor(body_size(), sinfo_size()); }
  void CodeIterateBody(ObjectVisitor* v);
#ifdef DEBUG
  void CodePrint();
  void CodeVerify();
#endif
  // Code entry points are aligned to 32 bytes.
  static const int kCodeAlignmentBits = 5;
  static const int kCodeAlignment = 1 << kCodeAlignmentBits;
  static const int kCodeAlignmentMask = kCodeAlignment - 1;

  // Layout description.
  static const int kInstructionSizeOffset = HeapObject::kHeaderSize;
  static const int kRelocationSizeOffset = kInstructionSizeOffset + kIntSize;
  static const int kSInfoSizeOffset = kRelocationSizeOffset + kIntSize;
  static const int kFlagsOffset = kSInfoSizeOffset + kIntSize;
  static const int kKindSpecificFlagsOffset  = kFlagsOffset + kIntSize;
  // Add padding to align the instruction start following right after
  // the Code object header.
  static const int kHeaderSize =
      (kKindSpecificFlagsOffset + kIntSize + kCodeAlignmentMask) &
          ~kCodeAlignmentMask;

  // Byte offsets within kKindSpecificFlagsOffset.
  static const int kStubMajorKeyOffset = kKindSpecificFlagsOffset + 1;

  // Flags layout.
  static const int kFlagsICStateShift        = 0;
  static const int kFlagsICInLoopShift       = 3;
  static const int kFlagsKindShift           = 4;
  static const int kFlagsTypeShift           = 7;
  static const int kFlagsArgumentsCountShift = 10;

  static const int kFlagsICStateMask        = 0x00000007;  // 0000000111
  static const int kFlagsICInLoopMask       = 0x00000008;  // 0000001000
  static const int kFlagsKindMask           = 0x00000070;  // 0001110000
  static const int kFlagsTypeMask           = 0x00000380;  // 1110000000
  static const int kFlagsArgumentsCountMask = 0xFFFFFC00;

  static const int kFlagsNotUsedInLookup =
      (kFlagsICInLoopMask | kFlagsTypeMask);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Code);
};


// All heap objects have a Map that describes their structure.
//  A Map contains information about:
//  - Size information about the object
//  - How to iterate over an object (for garbage collection)
class Map: public HeapObject {
 public:
  // Instance size.
  inline int instance_size();
  inline void set_instance_size(int value);

  // Count of properties allocated in the object.
  inline int inobject_properties();
  inline void set_inobject_properties(int value);

  // Count of property fields pre-allocated in the object when first allocated.
  inline int pre_allocated_property_fields();
  inline void set_pre_allocated_property_fields(int value);

  // Instance type.
  inline InstanceType instance_type();
  inline void set_instance_type(InstanceType value);

  // Tells how many unused property fields are available in the
  // instance (only used for JSObject in fast mode).
  inline int unused_property_fields();
  inline void set_unused_property_fields(int value);

  // Bit field.
  inline byte bit_field();
  inline void set_bit_field(byte value);

  // Bit field 2.
  inline byte bit_field2();
  inline void set_bit_field2(byte value);

  // Tells whether the object in the prototype property will be used
  // for instances created from this function.  If the prototype
  // property is set to a value that is not a JSObject, the prototype
  // property will not be used to create instances of the function.
  // See ECMA-262, 13.2.2.
  inline void set_non_instance_prototype(bool value);
  inline bool has_non_instance_prototype();

  // Tells whether the instance with this map should be ignored by the
  // __proto__ accessor.
  inline void set_is_hidden_prototype() {
    set_bit_field(bit_field() | (1 << kIsHiddenPrototype));
  }

  inline bool is_hidden_prototype() {
    return ((1 << kIsHiddenPrototype) & bit_field()) != 0;
  }

  // Records and queries whether the instance has a named interceptor.
  inline void set_has_named_interceptor() {
    set_bit_field(bit_field() | (1 << kHasNamedInterceptor));
  }

  inline bool has_named_interceptor() {
    return ((1 << kHasNamedInterceptor) & bit_field()) != 0;
  }

  // Records and queries whether the instance has an indexed interceptor.
  inline void set_has_indexed_interceptor() {
    set_bit_field(bit_field() | (1 << kHasIndexedInterceptor));
  }

  inline bool has_indexed_interceptor() {
    return ((1 << kHasIndexedInterceptor) & bit_field()) != 0;
  }

  // Tells whether the instance is undetectable.
  // An undetectable object is a special class of JSObject: 'typeof' operator
  // returns undefined, ToBoolean returns false. Otherwise it behaves like
  // a normal JS object.  It is useful for implementing undetectable
  // document.all in Firefox & Safari.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=248549.
  inline void set_is_undetectable() {
    set_bit_field(bit_field() | (1 << kIsUndetectable));
  }

  inline bool is_undetectable() {
    return ((1 << kIsUndetectable) & bit_field()) != 0;
  }

  inline void set_needs_loading(bool value) {
    if (value) {
      set_bit_field2(bit_field2() | (1 << kNeedsLoading));
    } else {
      set_bit_field2(bit_field2() & ~(1 << kNeedsLoading));
    }
  }

  // Does this object or function require a lazily loaded script to be
  // run before being used?
  inline bool needs_loading() {
    return ((1 << kNeedsLoading) & bit_field2()) != 0;
  }

  // Tells whether the instance has a call-as-function handler.
  inline void set_has_instance_call_handler() {
    set_bit_field(bit_field() | (1 << kHasInstanceCallHandler));
  }

  inline bool has_instance_call_handler() {
    return ((1 << kHasInstanceCallHandler) & bit_field()) != 0;
  }

  // Tells whether the instance needs security checks when accessing its
  // properties.
  inline void set_is_access_check_needed(bool access_check_needed);
  inline bool is_access_check_needed();

  // [prototype]: implicit prototype object.
  DECL_ACCESSORS(prototype, Object)

  // [constructor]: points back to the function responsible for this map.
  DECL_ACCESSORS(constructor, Object)

  // [instance descriptors]: describes the object.
  DECL_ACCESSORS(instance_descriptors, DescriptorArray)

  // [stub cache]: contains stubs compiled for this map.
  DECL_ACCESSORS(code_cache, FixedArray)

  // Returns a copy of the map.
  Object* CopyDropDescriptors();

  // Returns a copy of the map, with all transitions dropped from the
  // instance descriptors.
  Object* CopyDropTransitions();

  // Returns the property index for name (only valid for FAST MODE).
  int PropertyIndexFor(String* name);

  // Returns the next free property index (only valid for FAST MODE).
  int NextFreePropertyIndex();

  // Returns the number of properties described in instance_descriptors.
  int NumberOfDescribedProperties();

  // Casting.
  static inline Map* cast(Object* obj);

  // Locate an accessor in the instance descriptor.
  AccessorDescriptor* FindAccessor(String* name);

  // Code cache operations.

  // Clears the code cache.
  inline void ClearCodeCache();

  // Update code cache.
  Object* UpdateCodeCache(String* name, Code* code);

  // Returns the found code or undefined if absent.
  Object* FindInCodeCache(String* name, Code::Flags flags);

  // Returns the non-negative index of the code object if it is in the
  // cache and -1 otherwise.
  int IndexInCodeCache(Code* code);

  // Removes a code object from the code cache at the given index.
  void RemoveFromCodeCache(int index);

  // For every transition in this map, makes the transition's
  // target's prototype pointer point back to this map.
  // This is undone in MarkCompactCollector::ClearNonLiveTransitions().
  void CreateBackPointers();

  // Set all map transitions from this map to dead maps to null.
  // Also, restore the original prototype on the targets of these
  // transitions, so that we do not process this map again while
  // following back pointers.
  void ClearNonLiveTransitions(Object* real_prototype);

  // Dispatched behavior.
  void MapIterateBody(ObjectVisitor* v);
#ifdef DEBUG
  void MapPrint();
  void MapVerify();
#endif

  static const int kMaxPreAllocatedPropertyFields = 255;

  // Layout description.
  static const int kInstanceSizesOffset = HeapObject::kHeaderSize;
  static const int kInstanceAttributesOffset = kInstanceSizesOffset + kIntSize;
  static const int kPrototypeOffset = kInstanceAttributesOffset + kIntSize;
  static const int kConstructorOffset = kPrototypeOffset + kPointerSize;
  static const int kInstanceDescriptorsOffset =
      kConstructorOffset + kPointerSize;
  static const int kCodeCacheOffset = kInstanceDescriptorsOffset + kPointerSize;
  static const int kSize = kCodeCacheOffset + kPointerSize;

  // Byte offsets within kInstanceSizesOffset.
  static const int kInstanceSizeOffset = kInstanceSizesOffset + 0;
  static const int kInObjectPropertiesByte = 1;
  static const int kInObjectPropertiesOffset =
      kInstanceSizesOffset + kInObjectPropertiesByte;
  static const int kPreAllocatedPropertyFieldsByte = 2;
  static const int kPreAllocatedPropertyFieldsOffset =
      kInstanceSizesOffset + kPreAllocatedPropertyFieldsByte;
  // The byte at position 3 is not in use at the moment.

  // Byte offsets within kInstanceAttributesOffset attributes.
  static const int kInstanceTypeOffset = kInstanceAttributesOffset + 0;
  static const int kUnusedPropertyFieldsOffset = kInstanceAttributesOffset + 1;
  static const int kBitFieldOffset = kInstanceAttributesOffset + 2;
  static const int kBitField2Offset = kInstanceAttributesOffset + 3;

  STATIC_CHECK(kInstanceTypeOffset == Internals::kMapInstanceTypeOffset);

  // Bit positions for bit field.
  static const int kUnused = 0;  // To be used for marking recently used maps.
  static const int kHasNonInstancePrototype = 1;
  static const int kIsHiddenPrototype = 2;
  static const int kHasNamedInterceptor = 3;
  static const int kHasIndexedInterceptor = 4;
  static const int kIsUndetectable = 5;
  static const int kHasInstanceCallHandler = 6;
  static const int kIsAccessCheckNeeded = 7;

  // Bit positions for bit field 2
  static const int kNeedsLoading = 0;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Map);
};


// An abstract superclass, a marker class really, for simple structure classes.
// It doesn't carry much functionality but allows struct classes to me
// identified in the type system.
class Struct: public HeapObject {
 public:
  inline void InitializeBody(int object_size);
  static inline Struct* cast(Object* that);
};


// Script describes a script which has been added to the VM.
class Script: public Struct {
 public:
  // Script types.
  enum Type {
    TYPE_NATIVE = 0,
    TYPE_EXTENSION = 1,
    TYPE_NORMAL = 2
  };

  // Script compilation types.
  enum CompilationType {
    COMPILATION_TYPE_HOST = 0,
    COMPILATION_TYPE_EVAL = 1,
    COMPILATION_TYPE_JSON = 2
  };

  // [source]: the script source.
  DECL_ACCESSORS(source, Object)

  // [name]: the script name.
  DECL_ACCESSORS(name, Object)

  // [id]: the script id.
  DECL_ACCESSORS(id, Object)

  // [line_offset]: script line offset in resource from where it was extracted.
  DECL_ACCESSORS(line_offset, Smi)

  // [column_offset]: script column offset in resource from where it was
  // extracted.
  DECL_ACCESSORS(column_offset, Smi)

  // [data]: additional data associated with this script.
  DECL_ACCESSORS(data, Object)

  // [context_data]: context data for the context this script was compiled in.
  DECL_ACCESSORS(context_data, Object)

  // [wrapper]: the wrapper cache.
  DECL_ACCESSORS(wrapper, Proxy)

  // [type]: the script type.
  DECL_ACCESSORS(type, Smi)

  // [compilation]: how the the script was compiled.
  DECL_ACCESSORS(compilation_type, Smi)

  // [line_ends]: FixedArray of line ends positions.
  DECL_ACCESSORS(line_ends_fixed_array, Object)

  // [line_ends]: JSArray of line ends positions.
  DECL_ACCESSORS(line_ends_js_array, Object)

  // [eval_from_function]: for eval scripts the funcion from which eval was
  // called.
  DECL_ACCESSORS(eval_from_function, Object)

  // [eval_from_instructions_offset]: the instruction offset in the code for the
  // function from which eval was called where eval was called.
  DECL_ACCESSORS(eval_from_instructions_offset, Smi)

  static inline Script* cast(Object* obj);

  // If script source is an external string, check that the underlying
  // resource is accessible. Otherwise, always return true.
  inline bool HasValidSource();

#ifdef DEBUG
  void ScriptPrint();
  void ScriptVerify();
#endif

  static const int kSourceOffset = HeapObject::kHeaderSize;
  static const int kNameOffset = kSourceOffset + kPointerSize;
  static const int kLineOffsetOffset = kNameOffset + kPointerSize;
  static const int kColumnOffsetOffset = kLineOffsetOffset + kPointerSize;
  static const int kDataOffset = kColumnOffsetOffset + kPointerSize;
  static const int kContextOffset = kDataOffset + kPointerSize;
  static const int kWrapperOffset = kContextOffset + kPointerSize;
  static const int kTypeOffset = kWrapperOffset + kPointerSize;
  static const int kCompilationTypeOffset = kTypeOffset + kPointerSize;
  // We have the line ends array both in FixedArray form and in JSArray form.
  // The FixedArray form is useful when we don't have a context and so can't
  // create a JSArray.  The JSArray form is useful when we want to see the
  // array from JS code (e.g. debug-delay.js) which cannot handle unboxed
  // FixedArray objects.
  static const int kLineEndsFixedArrayOffset =
      kCompilationTypeOffset + kPointerSize;
  static const int kLineEndsJSArrayOffset =
      kLineEndsFixedArrayOffset + kPointerSize;
  static const int kIdOffset = kLineEndsJSArrayOffset + kPointerSize;
  static const int kEvalFromFunctionOffset = kIdOffset + kPointerSize;
  static const int kEvalFrominstructionsOffsetOffset =
      kEvalFromFunctionOffset + kPointerSize;
  static const int kSize = kEvalFrominstructionsOffsetOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Script);
};


// SharedFunctionInfo describes the JSFunction information that can be
// shared by multiple instances of the function.
class SharedFunctionInfo: public HeapObject {
 public:
  // [name]: Function name.
  DECL_ACCESSORS(name, Object)

  // [code]: Function code.
  DECL_ACCESSORS(code, Code)

  // [construct stub]: Code stub for constructing instances of this function.
  DECL_ACCESSORS(construct_stub, Code)

  // Returns if this function has been compiled to native code yet.
  inline bool is_compiled();

  // [length]: The function length - usually the number of declared parameters.
  // Use up to 2^30 parameters.
  inline int length();
  inline void set_length(int value);

  // [formal parameter count]: The declared number of parameters.
  inline int formal_parameter_count();
  inline void set_formal_parameter_count(int value);

  // Set the formal parameter count so the function code will be
  // called without using argument adaptor frames.
  inline void DontAdaptArguments();

  // [expected_nof_properties]: Expected number of properties for the function.
  inline int expected_nof_properties();
  inline void set_expected_nof_properties(int value);

  // [instance class name]: class name for instances.
  DECL_ACCESSORS(instance_class_name, Object)

  // [function data]: This field has been added for make benefit the API.
  // In the long run we don't want all functions to have this field but
  // we can fix that when we have a better model for storing hidden data
  // on objects.
  DECL_ACCESSORS(function_data, Object)

  // [script info]: Script from which the function originates.
  DECL_ACCESSORS(script, Object)

  // [start_position_and_type]: Field used to store both the source code
  // position, whether or not the function is a function expression,
  // and whether or not the function is a toplevel function. The two
  // least significants bit indicates whether the function is an
  // expression and the rest contains the source code position.
  inline int start_position_and_type();
  inline void set_start_position_and_type(int value);

  // [debug info]: Debug information.
  DECL_ACCESSORS(debug_info, Object)

  // [inferred name]: Name inferred from variable or property
  // assignment of this function. Used to facilitate debugging and
  // profiling of JavaScript code written in OO style, where almost
  // all functions are anonymous but are assigned to object
  // properties.
  DECL_ACCESSORS(inferred_name, String)

  // Position of the 'function' token in the script source.
  inline int function_token_position();
  inline void set_function_token_position(int function_token_position);

  // Position of this function in the script source.
  inline int start_position();
  inline void set_start_position(int start_position);

  // End position of this function in the script source.
  inline int end_position();
  inline void set_end_position(int end_position);

  // Is this function a function expression in the source code.
  inline bool is_expression();
  inline void set_is_expression(bool value);

  // Is this function a top-level function (scripts, evals).
  inline bool is_toplevel();
  inline void set_is_toplevel(bool value);

  // Bit field containing various information collected by the compiler to
  // drive optimization.
  inline int compiler_hints();
  inline void set_compiler_hints(int value);

  // Add information on assignments of the form this.x = ...;
  void SetThisPropertyAssignmentsInfo(
      bool has_only_simple_this_property_assignments,
      FixedArray* this_property_assignments);

  // Clear information on assignments of the form this.x = ...;
  void ClearThisPropertyAssignmentsInfo();

  // Indicate that this function only consists of assignments of the form
  // this.x = y; where y is either a constant or refers to an argument.
  inline bool has_only_simple_this_property_assignments();

  inline bool try_fast_codegen();
  inline void set_try_fast_codegen(bool flag);

  // For functions which only contains this property assignments this provides
  // access to the names for the properties assigned.
  DECL_ACCESSORS(this_property_assignments, Object)
  inline int this_property_assignments_count();
  inline void set_this_property_assignments_count(int value);
  String* GetThisPropertyAssignmentName(int index);
  bool IsThisPropertyAssignmentArgument(int index);
  int GetThisPropertyAssignmentArgument(int index);
  Object* GetThisPropertyAssignmentConstant(int index);

  // [source code]: Source code for the function.
  bool HasSourceCode();
  Object* GetSourceCode();

  // Calculate the instance size.
  int CalculateInstanceSize();

  // Calculate the number of in-object properties.
  int CalculateInObjectProperties();

  // Dispatched behavior.
  void SharedFunctionInfoIterateBody(ObjectVisitor* v);
  // Set max_length to -1 for unlimited length.
  void SourceCodePrint(StringStream* accumulator, int max_length);
#ifdef DEBUG
  void SharedFunctionInfoPrint();
  void SharedFunctionInfoVerify();
#endif

  // Casting.
  static inline SharedFunctionInfo* cast(Object* obj);

  // Constants.
  static const int kDontAdaptArgumentsSentinel = -1;

  // Layout description.
  // (An even number of integers has a size that is a multiple of a pointer.)
  static const int kNameOffset = HeapObject::kHeaderSize;
  static const int kCodeOffset = kNameOffset + kPointerSize;
  static const int kConstructStubOffset = kCodeOffset + kPointerSize;
  static const int kLengthOffset = kConstructStubOffset + kPointerSize;
  static const int kFormalParameterCountOffset = kLengthOffset + kIntSize;
  static const int kExpectedNofPropertiesOffset =
      kFormalParameterCountOffset + kIntSize;
  static const int kStartPositionAndTypeOffset =
      kExpectedNofPropertiesOffset + kIntSize;
  static const int kEndPositionOffset = kStartPositionAndTypeOffset + kIntSize;
  static const int kFunctionTokenPositionOffset = kEndPositionOffset + kIntSize;
  static const int kInstanceClassNameOffset =
      kFunctionTokenPositionOffset + kIntSize;
  static const int kExternalReferenceDataOffset =
      kInstanceClassNameOffset + kPointerSize;
  static const int kScriptOffset = kExternalReferenceDataOffset + kPointerSize;
  static const int kDebugInfoOffset = kScriptOffset + kPointerSize;
  static const int kInferredNameOffset = kDebugInfoOffset + kPointerSize;
  static const int kCompilerHintsOffset = kInferredNameOffset + kPointerSize;
  static const int kThisPropertyAssignmentsOffset =
      kCompilerHintsOffset + kPointerSize;
  static const int kThisPropertyAssignmentsCountOffset =
      kThisPropertyAssignmentsOffset + kPointerSize;
  static const int kSize = kThisPropertyAssignmentsCountOffset + kPointerSize;

 private:
  // Bit positions in length_and_flg.
  // The least significant bit is used as the flag.
  static const int kFlagBit         = 0;
  static const int kLengthShift     = 1;
  static const int kLengthMask      = ~((1 << kLengthShift) - 1);

  // Bit positions in start_position_and_type.
  // The source code start position is in the 30 most significant bits of
  // the start_position_and_type field.
  static const int kIsExpressionBit = 0;
  static const int kIsTopLevelBit   = 1;
  static const int kStartPositionShift = 2;
  static const int kStartPositionMask = ~((1 << kStartPositionShift) - 1);

  // Bit positions in compiler_hints.
  static const int kHasOnlySimpleThisPropertyAssignments = 0;
  static const int kTryFastCodegen = 1;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SharedFunctionInfo);
};


// JSFunction describes JavaScript functions.
class JSFunction: public JSObject {
 public:
  // [prototype_or_initial_map]:
  DECL_ACCESSORS(prototype_or_initial_map, Object)

  // [shared_function_info]: The information about the function that
  // can be shared by instances.
  DECL_ACCESSORS(shared, SharedFunctionInfo)

  // [context]: The context for this function.
  inline Context* context();
  inline Object* unchecked_context();
  inline void set_context(Object* context);

  // [code]: The generated code object for this function.  Executed
  // when the function is invoked, e.g. foo() or new foo(). See
  // [[Call]] and [[Construct]] description in ECMA-262, section
  // 8.6.2, page 27.
  inline Code* code();
  inline void set_code(Code* value);

  // Tells whether this function is a context-independent boilerplate
  // function.
  inline bool IsBoilerplate();

  // Tells whether this function is builtin.
  inline bool IsBuiltin();

  // [literals]: Fixed array holding the materialized literals.
  //
  // If the function contains object, regexp or array literals, the
  // literals array prefix contains the object, regexp, and array
  // function to be used when creating these literals.  This is
  // necessary so that we do not dynamically lookup the object, regexp
  // or array functions.  Performing a dynamic lookup, we might end up
  // using the functions from a new context that we should not have
  // access to.
  DECL_ACCESSORS(literals, FixedArray)

  // The initial map for an object created by this constructor.
  inline Map* initial_map();
  inline void set_initial_map(Map* value);
  inline bool has_initial_map();

  // Get and set the prototype property on a JSFunction. If the
  // function has an initial map the prototype is set on the initial
  // map. Otherwise, the prototype is put in the initial map field
  // until an initial map is needed.
  inline bool has_prototype();
  inline bool has_instance_prototype();
  inline Object* prototype();
  inline Object* instance_prototype();
  Object* SetInstancePrototype(Object* value);
  Object* SetPrototype(Object* value);

  // Accessor for this function's initial map's [[class]]
  // property. This is primarily used by ECMA native functions.  This
  // method sets the class_name field of this function's initial map
  // to a given value. It creates an initial map if this function does
  // not have one. Note that this method does not copy the initial map
  // if it has one already, but simply replaces it with the new value.
  // Instances created afterwards will have a map whose [[class]] is
  // set to 'value', but there is no guarantees on instances created
  // before.
  Object* SetInstanceClassName(String* name);

  // Returns if this function has been compiled to native code yet.
  inline bool is_compiled();

  // Casting.
  static inline JSFunction* cast(Object* obj);

  // Dispatched behavior.
#ifdef DEBUG
  void JSFunctionPrint();
  void JSFunctionVerify();
#endif

  // Returns the number of allocated literals.
  inline int NumberOfLiterals();

  // Retrieve the global context from a function's literal array.
  static Context* GlobalContextFromLiterals(FixedArray* literals);

  // Layout descriptors.
  static const int kPrototypeOrInitialMapOffset = JSObject::kHeaderSize;
  static const int kSharedFunctionInfoOffset =
      kPrototypeOrInitialMapOffset + kPointerSize;
  static const int kContextOffset = kSharedFunctionInfoOffset + kPointerSize;
  static const int kLiteralsOffset = kContextOffset + kPointerSize;
  static const int kSize = kLiteralsOffset + kPointerSize;

  // Layout of the literals array.
  static const int kLiteralsPrefixSize = 1;
  static const int kLiteralGlobalContextIndex = 0;
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSFunction);
};


// JSGlobalProxy's prototype must be a JSGlobalObject or null,
// and the prototype is hidden. JSGlobalProxy always delegates
// property accesses to its prototype if the prototype is not null.
//
// A JSGlobalProxy can be reinitialized which will preserve its identity.
//
// Accessing a JSGlobalProxy requires security check.

class JSGlobalProxy : public JSObject {
 public:
  // [context]: the owner global context of this proxy object.
  // It is null value if this object is not used by any context.
  DECL_ACCESSORS(context, Object)

  // Casting.
  static inline JSGlobalProxy* cast(Object* obj);

  // Dispatched behavior.
#ifdef DEBUG
  void JSGlobalProxyPrint();
  void JSGlobalProxyVerify();
#endif

  // Layout description.
  static const int kContextOffset = JSObject::kHeaderSize;
  static const int kSize = kContextOffset + kPointerSize;

 private:

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSGlobalProxy);
};


// Forward declaration.
class JSBuiltinsObject;

// Common super class for JavaScript global objects and the special
// builtins global objects.
class GlobalObject: public JSObject {
 public:
  // [builtins]: the object holding the runtime routines written in JS.
  DECL_ACCESSORS(builtins, JSBuiltinsObject)

  // [global context]: the global context corresponding to this global object.
  DECL_ACCESSORS(global_context, Context)

  // [global receiver]: the global receiver object of the context
  DECL_ACCESSORS(global_receiver, JSObject)

  // Retrieve the property cell used to store a property.
  Object* GetPropertyCell(LookupResult* result);

  // Ensure that the global object has a cell for the given property name.
  Object* EnsurePropertyCell(String* name);

  // Casting.
  static inline GlobalObject* cast(Object* obj);

  // Layout description.
  static const int kBuiltinsOffset = JSObject::kHeaderSize;
  static const int kGlobalContextOffset = kBuiltinsOffset + kPointerSize;
  static const int kGlobalReceiverOffset = kGlobalContextOffset + kPointerSize;
  static const int kHeaderSize = kGlobalReceiverOffset + kPointerSize;

 private:
  friend class AGCCVersionRequiresThisClassToHaveAFriendSoHereItIs;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GlobalObject);
};


// JavaScript global object.
class JSGlobalObject: public GlobalObject {
 public:

  // Casting.
  static inline JSGlobalObject* cast(Object* obj);

  // Dispatched behavior.
#ifdef DEBUG
  void JSGlobalObjectPrint();
  void JSGlobalObjectVerify();
#endif

  // Layout description.
  static const int kSize = GlobalObject::kHeaderSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSGlobalObject);
};


// Builtins global object which holds the runtime routines written in
// JavaScript.
class JSBuiltinsObject: public GlobalObject {
 public:
  // Accessors for the runtime routines written in JavaScript.
  inline Object* javascript_builtin(Builtins::JavaScript id);
  inline void set_javascript_builtin(Builtins::JavaScript id, Object* value);

  // Casting.
  static inline JSBuiltinsObject* cast(Object* obj);

  // Dispatched behavior.
#ifdef DEBUG
  void JSBuiltinsObjectPrint();
  void JSBuiltinsObjectVerify();
#endif

  // Layout description.  The size of the builtins object includes
  // room for one pointer per runtime routine written in javascript.
  static const int kJSBuiltinsCount = Builtins::id_count;
  static const int kJSBuiltinsOffset = GlobalObject::kHeaderSize;
  static const int kSize =
      kJSBuiltinsOffset + (kJSBuiltinsCount * kPointerSize);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSBuiltinsObject);
};


// Representation for JS Wrapper objects, String, Number, Boolean, Date, etc.
class JSValue: public JSObject {
 public:
  // [value]: the object being wrapped.
  DECL_ACCESSORS(value, Object)

  // Casting.
  static inline JSValue* cast(Object* obj);

  // Dispatched behavior.
#ifdef DEBUG
  void JSValuePrint();
  void JSValueVerify();
#endif

  // Layout description.
  static const int kValueOffset = JSObject::kHeaderSize;
  static const int kSize = kValueOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSValue);
};

// Regular expressions
// The regular expression holds a single reference to a FixedArray in
// the kDataOffset field.
// The FixedArray contains the following data:
// - tag : type of regexp implementation (not compiled yet, atom or irregexp)
// - reference to the original source string
// - reference to the original flag string
// If it is an atom regexp
// - a reference to a literal string to search for
// If it is an irregexp regexp:
// - a reference to code for ASCII inputs (bytecode or compiled).
// - a reference to code for UC16 inputs (bytecode or compiled).
// - max number of registers used by irregexp implementations.
// - number of capture registers (output values) of the regexp.
class JSRegExp: public JSObject {
 public:
  // Meaning of Type:
  // NOT_COMPILED: Initial value. No data has been stored in the JSRegExp yet.
  // ATOM: A simple string to match against using an indexOf operation.
  // IRREGEXP: Compiled with Irregexp.
  // IRREGEXP_NATIVE: Compiled to native code with Irregexp.
  enum Type { NOT_COMPILED, ATOM, IRREGEXP };
  enum Flag { NONE = 0, GLOBAL = 1, IGNORE_CASE = 2, MULTILINE = 4 };

  class Flags {
   public:
    explicit Flags(uint32_t value) : value_(value) { }
    bool is_global() { return (value_ & GLOBAL) != 0; }
    bool is_ignore_case() { return (value_ & IGNORE_CASE) != 0; }
    bool is_multiline() { return (value_ & MULTILINE) != 0; }
    uint32_t value() { return value_; }
   private:
    uint32_t value_;
  };

  DECL_ACCESSORS(data, Object)

  inline Type TypeTag();
  inline int CaptureCount();
  inline Flags GetFlags();
  inline String* Pattern();
  inline Object* DataAt(int index);
  // Set implementation data after the object has been prepared.
  inline void SetDataAt(int index, Object* value);
  static int code_index(bool is_ascii) {
    if (is_ascii) {
      return kIrregexpASCIICodeIndex;
    } else {
      return kIrregexpUC16CodeIndex;
    }
  }

  static inline JSRegExp* cast(Object* obj);

  // Dispatched behavior.
#ifdef DEBUG
  void JSRegExpVerify();
#endif

  static const int kDataOffset = JSObject::kHeaderSize;
  static const int kSize = kDataOffset + kPointerSize;

  // Indices in the data array.
  static const int kTagIndex = 0;
  static const int kSourceIndex = kTagIndex + 1;
  static const int kFlagsIndex = kSourceIndex + 1;
  static const int kDataIndex = kFlagsIndex + 1;
  // The data fields are used in different ways depending on the
  // value of the tag.
  // Atom regexps (literal strings).
  static const int kAtomPatternIndex = kDataIndex;

  static const int kAtomDataSize = kAtomPatternIndex + 1;

  // Irregexp compiled code or bytecode for ASCII. If compilation
  // fails, this fields hold an exception object that should be
  // thrown if the regexp is used again.
  static const int kIrregexpASCIICodeIndex = kDataIndex;
  // Irregexp compiled code or bytecode for UC16.  If compilation
  // fails, this fields hold an exception object that should be
  // thrown if the regexp is used again.
  static const int kIrregexpUC16CodeIndex = kDataIndex + 1;
  // Maximal number of registers used by either ASCII or UC16.
  // Only used to check that there is enough stack space
  static const int kIrregexpMaxRegisterCountIndex = kDataIndex + 2;
  // Number of captures in the compiled regexp.
  static const int kIrregexpCaptureCountIndex = kDataIndex + 3;

  static const int kIrregexpDataSize = kIrregexpCaptureCountIndex + 1;
};


class CompilationCacheShape {
 public:
  static inline bool IsMatch(HashTableKey* key, Object* value) {
    return key->IsMatch(value);
  }

  static inline uint32_t Hash(HashTableKey* key) {
    return key->Hash();
  }

  static inline uint32_t HashForObject(HashTableKey* key, Object* object) {
    return key->HashForObject(object);
  }

  static Object* AsObject(HashTableKey* key) {
    return key->AsObject();
  }

  static const int kPrefixSize = 0;
  static const int kEntrySize = 2;
};


class CompilationCacheTable: public HashTable<CompilationCacheShape,
                                              HashTableKey*> {
 public:
  // Find cached value for a string key, otherwise return null.
  Object* Lookup(String* src);
  Object* LookupEval(String* src, Context* context);
  Object* LookupRegExp(String* source, JSRegExp::Flags flags);
  Object* Put(String* src, Object* value);
  Object* PutEval(String* src, Context* context, Object* value);
  Object* PutRegExp(String* src, JSRegExp::Flags flags, FixedArray* value);

  static inline CompilationCacheTable* cast(Object* obj);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheTable);
};


enum AllowNullsFlag {ALLOW_NULLS, DISALLOW_NULLS};
enum RobustnessFlag {ROBUST_STRING_TRAVERSAL, FAST_STRING_TRAVERSAL};


class StringHasher {
 public:
  inline StringHasher(int length);

  // Returns true if the hash of this string can be computed without
  // looking at the contents.
  inline bool has_trivial_hash();

  // Add a character to the hash and update the array index calculation.
  inline void AddCharacter(uc32 c);

  // Adds a character to the hash but does not update the array index
  // calculation.  This can only be called when it has been verified
  // that the input is not an array index.
  inline void AddCharacterNoIndex(uc32 c);

  // Returns the value to store in the hash field of a string with
  // the given length and contents.
  uint32_t GetHashField();

  // Returns true if the characters seen so far make up a legal array
  // index.
  bool is_array_index() { return is_array_index_; }

  bool is_valid() { return is_valid_; }

  void invalidate() { is_valid_ = false; }

 private:

  uint32_t array_index() {
    ASSERT(is_array_index());
    return array_index_;
  }

  inline uint32_t GetHash();

  int length_;
  uint32_t raw_running_hash_;
  uint32_t array_index_;
  bool is_array_index_;
  bool is_first_char_;
  bool is_valid_;
  friend class TwoCharHashTableKey;
};


// The characteristics of a string are stored in its map.  Retrieving these
// few bits of information is moderately expensive, involving two memory
// loads where the second is dependent on the first.  To improve efficiency
// the shape of the string is given its own class so that it can be retrieved
// once and used for several string operations.  A StringShape is small enough
// to be passed by value and is immutable, but be aware that flattening a
// string can potentially alter its shape.  Also be aware that a GC caused by
// something else can alter the shape of a string due to ConsString
// shortcutting.  Keeping these restrictions in mind has proven to be error-
// prone and so we no longer put StringShapes in variables unless there is a
// concrete performance benefit at that particular point in the code.
class StringShape BASE_EMBEDDED {
 public:
  inline explicit StringShape(String* s);
  inline explicit StringShape(Map* s);
  inline explicit StringShape(InstanceType t);
  inline bool IsSequential();
  inline bool IsExternal();
  inline bool IsCons();
  inline bool IsExternalAscii();
  inline bool IsExternalTwoByte();
  inline bool IsSequentialAscii();
  inline bool IsSequentialTwoByte();
  inline bool IsSymbol();
  inline StringRepresentationTag representation_tag();
  inline uint32_t full_representation_tag();
  inline uint32_t size_tag();
#ifdef DEBUG
  inline uint32_t type() { return type_; }
  inline void invalidate() { valid_ = false; }
  inline bool valid() { return valid_; }
#else
  inline void invalidate() { }
#endif
 private:
  uint32_t type_;
#ifdef DEBUG
  inline void set_valid() { valid_ = true; }
  bool valid_;
#else
  inline void set_valid() { }
#endif
};


// The String abstract class captures JavaScript string values:
//
// Ecma-262:
//  4.3.16 String Value
//    A string value is a member of the type String and is a finite
//    ordered sequence of zero or more 16-bit unsigned integer values.
//
// All string values have a length field.
class String: public HeapObject {
 public:
  // Get and set the length of the string.
  inline int length();
  inline void set_length(int value);

  // Get and set the uninterpreted length field of the string.  Notice
  // that the length field is also used to cache the hash value of
  // strings.  In order to get or set the actual length of the string
  // use the length() and set_length methods.
  inline uint32_t length_field();
  inline void set_length_field(uint32_t value);

  inline bool IsAsciiRepresentation();
  inline bool IsTwoByteRepresentation();

  // Get and set individual two byte chars in the string.
  inline void Set(int index, uint16_t value);
  // Get individual two byte char in the string.  Repeated calls
  // to this method are not efficient unless the string is flat.
  inline uint16_t Get(int index);

  // Try to flatten the top level ConsString that is hiding behind this
  // string.  This is a no-op unless the string is a ConsString.  Flatten
  // mutates the ConsString and might return a failure.
  Object* TryFlatten();

  // Try to flatten the string.  Checks first inline to see if it is necessary.
  // Do not handle allocation failures.  After calling TryFlattenIfNotFlat, the
  // string could still be a ConsString, in which case a failure is returned.
  // Use FlattenString from Handles.cc to be sure to flatten.
  inline Object* TryFlattenIfNotFlat();

  Vector<const char> ToAsciiVector();
  Vector<const uc16> ToUC16Vector();

  // Mark the string as an undetectable object. It only applies to
  // ascii and two byte string types.
  bool MarkAsUndetectable();

  // Return a substring.
  Object* SubString(int from, int to);

  // String equality operations.
  inline bool Equals(String* other);
  bool IsEqualTo(Vector<const char> str);

  // Return a UTF8 representation of the string.  The string is null
  // terminated but may optionally contain nulls.  Length is returned
  // in length_output if length_output is not a null pointer  The string
  // should be nearly flat, otherwise the performance of this method may
  // be very slow (quadratic in the length).  Setting robustness_flag to
  // ROBUST_STRING_TRAVERSAL invokes behaviour that is robust  This means it
  // handles unexpected data without causing assert failures and it does not
  // do any heap allocations.  This is useful when printing stack traces.
  SmartPointer<char> ToCString(AllowNullsFlag allow_nulls,
                               RobustnessFlag robustness_flag,
                               int offset,
                               int length,
                               int* length_output = 0);
  SmartPointer<char> ToCString(
      AllowNullsFlag allow_nulls = DISALLOW_NULLS,
      RobustnessFlag robustness_flag = FAST_STRING_TRAVERSAL,
      int* length_output = 0);

  int Utf8Length();

  // Return a 16 bit Unicode representation of the string.
  // The string should be nearly flat, otherwise the performance of
  // of this method may be very bad.  Setting robustness_flag to
  // ROBUST_STRING_TRAVERSAL invokes behaviour that is robust  This means it
  // handles unexpected data without causing assert failures and it does not
  // do any heap allocations.  This is useful when printing stack traces.
  SmartPointer<uc16> ToWideCString(
      RobustnessFlag robustness_flag = FAST_STRING_TRAVERSAL);

  // Tells whether the hash code has been computed.
  inline bool HasHashCode();

  // Returns a hash value used for the property table
  inline uint32_t Hash();

  static uint32_t ComputeLengthAndHashField(unibrow::CharacterStream* buffer,
                                            int length);

  static bool ComputeArrayIndex(unibrow::CharacterStream* buffer,
                                uint32_t* index,
                                int length);

  // Externalization.
  bool MakeExternal(v8::String::ExternalStringResource* resource);
  bool MakeExternal(v8::String::ExternalAsciiStringResource* resource);

  // Conversion.
  inline bool AsArrayIndex(uint32_t* index);

  // Casting.
  static inline String* cast(Object* obj);

  void PrintOn(FILE* out);

  // For use during stack traces.  Performs rudimentary sanity check.
  bool LooksValid();

  // Dispatched behavior.
  void StringShortPrint(StringStream* accumulator);
#ifdef DEBUG
  void StringPrint();
  void StringVerify();
#endif
  inline bool IsFlat();

  // Layout description.
  static const int kLengthOffset = HeapObject::kHeaderSize;
  static const int kSize = kLengthOffset + kIntSize;
  // Notice: kSize is not pointer-size aligned if pointers are 64-bit.

  // Maximum number of characters to consider when trying to convert a string
  // value into an array index.
  static const int kMaxArrayIndexSize = 10;

  // Max ascii char code.
  static const int kMaxAsciiCharCode = unibrow::Utf8::kMaxOneByteChar;
  static const unsigned kMaxAsciiCharCodeU = unibrow::Utf8::kMaxOneByteChar;
  static const int kMaxUC16CharCode = 0xffff;

  // Minimum length for a cons string.
  static const int kMinNonFlatLength = 13;

  // Mask constant for checking if a string has a computed hash code
  // and if it is an array index.  The least significant bit indicates
  // whether a hash code has been computed.  If the hash code has been
  // computed the 2nd bit tells whether the string can be used as an
  // array index.
  static const int kHashComputedMask = 1;
  static const int kIsArrayIndexMask = 1 << 1;
  static const int kNofLengthBitFields = 2;

  // Array index strings this short can keep their index in the hash
  // field.
  static const int kMaxCachedArrayIndexLength = 7;

  // Shift constants for retrieving length and hash code from
  // length/hash field.
  static const int kHashShift = kNofLengthBitFields;
  static const int kShortLengthShift = kHashShift + kShortStringTag;
  static const int kMediumLengthShift = kHashShift + kMediumStringTag;
  static const int kLongLengthShift = kHashShift + kLongStringTag;

  // Maximal string length that can be stored in the hash/length field for
  // different types of strings.
  static const int kMaxShortSize = (1 << (32 - kShortLengthShift)) - 1;
  static const int kMaxMediumSize = (1 << (32 - kMediumLengthShift)) - 1;
  static const int kMaxLength = (1 << (32 - kLongLengthShift)) - 1;

  // Limit for truncation in short printing.
  static const int kMaxShortPrintLength = 1024;

  // Support for regular expressions.
  const uc16* GetTwoByteData();
  const uc16* GetTwoByteData(unsigned start);

  // Support for StringInputBuffer
  static const unibrow::byte* ReadBlock(String* input,
                                        unibrow::byte* util_buffer,
                                        unsigned capacity,
                                        unsigned* remaining,
                                        unsigned* offset);
  static const unibrow::byte* ReadBlock(String** input,
                                        unibrow::byte* util_buffer,
                                        unsigned capacity,
                                        unsigned* remaining,
                                        unsigned* offset);

  // Helper function for flattening strings.
  template <typename sinkchar>
  static void WriteToFlat(String* source,
                          sinkchar* sink,
                          int from,
                          int to);

 protected:
  class ReadBlockBuffer {
   public:
    ReadBlockBuffer(unibrow::byte* util_buffer_,
                    unsigned cursor_,
                    unsigned capacity_,
                    unsigned remaining_) :
      util_buffer(util_buffer_),
      cursor(cursor_),
      capacity(capacity_),
      remaining(remaining_) {
    }
    unibrow::byte* util_buffer;
    unsigned       cursor;
    unsigned       capacity;
    unsigned       remaining;
  };

  static inline const unibrow::byte* ReadBlock(String* input,
                                               ReadBlockBuffer* buffer,
                                               unsigned* offset,
                                               unsigned max_chars);
  static void ReadBlockIntoBuffer(String* input,
                                  ReadBlockBuffer* buffer,
                                  unsigned* offset_ptr,
                                  unsigned max_chars);

 private:
  // Slow case of String::Equals.  This implementation works on any strings
  // but it is most efficient on strings that are almost flat.
  bool SlowEquals(String* other);

  // Slow case of AsArrayIndex.
  bool SlowAsArrayIndex(uint32_t* index);

  // Compute and set the hash code.
  uint32_t ComputeAndSetHash();

  DISALLOW_IMPLICIT_CONSTRUCTORS(String);
};


// The SeqString abstract class captures sequential string values.
class SeqString: public String {
 public:

  // Casting.
  static inline SeqString* cast(Object* obj);

  // Dispatched behaviour.
  // For regexp code.
  uint16_t* SeqStringGetTwoByteAddress();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SeqString);
};


// The AsciiString class captures sequential ascii string objects.
// Each character in the AsciiString is an ascii character.
class SeqAsciiString: public SeqString {
 public:
  // Dispatched behavior.
  inline uint16_t SeqAsciiStringGet(int index);
  inline void SeqAsciiStringSet(int index, uint16_t value);

  // Get the address of the characters in this string.
  inline Address GetCharsAddress();

  inline char* GetChars();

  // Casting
  static inline SeqAsciiString* cast(Object* obj);

  // Garbage collection support.  This method is called by the
  // garbage collector to compute the actual size of an AsciiString
  // instance.
  inline int SeqAsciiStringSize(InstanceType instance_type);

  // Computes the size for an AsciiString instance of a given length.
  static int SizeFor(int length) {
    return OBJECT_SIZE_ALIGN(kHeaderSize + length * kCharSize);
  }

  // Layout description.
  static const int kHeaderSize = String::kSize;
  static const int kAlignedSize = POINTER_SIZE_ALIGN(kHeaderSize);

  // Support for StringInputBuffer.
  inline void SeqAsciiStringReadBlockIntoBuffer(ReadBlockBuffer* buffer,
                                                unsigned* offset,
                                                unsigned chars);
  inline const unibrow::byte* SeqAsciiStringReadBlock(unsigned* remaining,
                                                      unsigned* offset,
                                                      unsigned chars);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SeqAsciiString);
};


// The TwoByteString class captures sequential unicode string objects.
// Each character in the TwoByteString is a two-byte uint16_t.
class SeqTwoByteString: public SeqString {
 public:
  // Dispatched behavior.
  inline uint16_t SeqTwoByteStringGet(int index);
  inline void SeqTwoByteStringSet(int index, uint16_t value);

  // Get the address of the characters in this string.
  inline Address GetCharsAddress();

  inline uc16* GetChars();

  // For regexp code.
  const uint16_t* SeqTwoByteStringGetData(unsigned start);

  // Casting
  static inline SeqTwoByteString* cast(Object* obj);

  // Garbage collection support.  This method is called by the
  // garbage collector to compute the actual size of a TwoByteString
  // instance.
  inline int SeqTwoByteStringSize(InstanceType instance_type);

  // Computes the size for a TwoByteString instance of a given length.
  static int SizeFor(int length) {
    return OBJECT_SIZE_ALIGN(kHeaderSize + length * kShortSize);
  }

  // Layout description.
  static const int kHeaderSize = String::kSize;
  static const int kAlignedSize = POINTER_SIZE_ALIGN(kHeaderSize);

  // Support for StringInputBuffer.
  inline void SeqTwoByteStringReadBlockIntoBuffer(ReadBlockBuffer* buffer,
                                                  unsigned* offset_ptr,
                                                  unsigned chars);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SeqTwoByteString);
};


// The ConsString class describes string values built by using the
// addition operator on strings.  A ConsString is a pair where the
// first and second components are pointers to other string values.
// One or both components of a ConsString can be pointers to other
// ConsStrings, creating a binary tree of ConsStrings where the leaves
// are non-ConsString string values.  The string value represented by
// a ConsString can be obtained by concatenating the leaf string
// values in a left-to-right depth-first traversal of the tree.
class ConsString: public String {
 public:
  // First string of the cons cell.
  inline String* first();
  // Doesn't check that the result is a string, even in debug mode.  This is
  // useful during GC where the mark bits confuse the checks.
  inline Object* unchecked_first();
  inline void set_first(String* first,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Second string of the cons cell.
  inline String* second();
  // Doesn't check that the result is a string, even in debug mode.  This is
  // useful during GC where the mark bits confuse the checks.
  inline Object* unchecked_second();
  inline void set_second(String* second,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Dispatched behavior.
  uint16_t ConsStringGet(int index);

  // Casting.
  static inline ConsString* cast(Object* obj);

  // Garbage collection support.  This method is called during garbage
  // collection to iterate through the heap pointers in the body of
  // the ConsString.
  void ConsStringIterateBody(ObjectVisitor* v);

  // Layout description.
  static const int kFirstOffset = POINTER_SIZE_ALIGN(String::kSize);
  static const int kSecondOffset = kFirstOffset + kPointerSize;
  static const int kSize = kSecondOffset + kPointerSize;

  // Support for StringInputBuffer.
  inline const unibrow::byte* ConsStringReadBlock(ReadBlockBuffer* buffer,
                                                  unsigned* offset_ptr,
                                                  unsigned chars);
  inline void ConsStringReadBlockIntoBuffer(ReadBlockBuffer* buffer,
                                            unsigned* offset_ptr,
                                            unsigned chars);

  // Minimum length for a cons string.
  static const int kMinLength = 13;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ConsString);
};


// The ExternalString class describes string values that are backed by
// a string resource that lies outside the V8 heap.  ExternalStrings
// consist of the length field common to all strings, a pointer to the
// external resource.  It is important to ensure (externally) that the
// resource is not deallocated while the ExternalString is live in the
// V8 heap.
//
// The API expects that all ExternalStrings are created through the
// API.  Therefore, ExternalStrings should not be used internally.
class ExternalString: public String {
 public:
  // Casting
  static inline ExternalString* cast(Object* obj);

  // Layout description.
  static const int kResourceOffset = POINTER_SIZE_ALIGN(String::kSize);
  static const int kSize = kResourceOffset + kPointerSize;

  STATIC_CHECK(kResourceOffset == Internals::kStringResourceOffset);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalString);
};


// The ExternalAsciiString class is an external string backed by an
// ASCII string.
class ExternalAsciiString: public ExternalString {
 public:
  typedef v8::String::ExternalAsciiStringResource Resource;

  // The underlying resource.
  inline Resource* resource();
  inline void set_resource(Resource* buffer);

  // Dispatched behavior.
  uint16_t ExternalAsciiStringGet(int index);

  // Casting.
  static inline ExternalAsciiString* cast(Object* obj);

  // Garbage collection support.
  void ExternalAsciiStringIterateBody(ObjectVisitor* v);

  // Support for StringInputBuffer.
  const unibrow::byte* ExternalAsciiStringReadBlock(unsigned* remaining,
                                                    unsigned* offset,
                                                    unsigned chars);
  inline void ExternalAsciiStringReadBlockIntoBuffer(ReadBlockBuffer* buffer,
                                                     unsigned* offset,
                                                     unsigned chars);

  // Identify the map for the external string/symbol with a particular length.
  static inline Map* StringMap(int length);
  static inline Map* SymbolMap(int length);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalAsciiString);
};


// The ExternalTwoByteString class is an external string backed by a UTF-16
// encoded string.
class ExternalTwoByteString: public ExternalString {
 public:
  typedef v8::String::ExternalStringResource Resource;

  // The underlying string resource.
  inline Resource* resource();
  inline void set_resource(Resource* buffer);

  // Dispatched behavior.
  uint16_t ExternalTwoByteStringGet(int index);

  // For regexp code.
  const uint16_t* ExternalTwoByteStringGetData(unsigned start);

  // Casting.
  static inline ExternalTwoByteString* cast(Object* obj);

  // Garbage collection support.
  void ExternalTwoByteStringIterateBody(ObjectVisitor* v);

  // Support for StringInputBuffer.
  void ExternalTwoByteStringReadBlockIntoBuffer(ReadBlockBuffer* buffer,
                                                unsigned* offset_ptr,
                                                unsigned chars);

  // Identify the map for the external string/symbol with a particular length.
  static inline Map* StringMap(int length);
  static inline Map* SymbolMap(int length);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalTwoByteString);
};


// Utility superclass for stack-allocated objects that must be updated
// on gc.  It provides two ways for the gc to update instances, either
// iterating or updating after gc.
class Relocatable BASE_EMBEDDED {
 public:
  inline Relocatable() : prev_(top_) { top_ = this; }
  virtual ~Relocatable() {
    ASSERT_EQ(top_, this);
    top_ = prev_;
  }
  virtual void IterateInstance(ObjectVisitor* v) { }
  virtual void PostGarbageCollection() { }

  static void PostGarbageCollectionProcessing();
  static int ArchiveSpacePerThread();
  static char* ArchiveState(char* to);
  static char* RestoreState(char* from);
  static void Iterate(ObjectVisitor* v);
  static void Iterate(ObjectVisitor* v, Relocatable* top);
  static char* Iterate(ObjectVisitor* v, char* t);
 private:
  static Relocatable* top_;
  Relocatable* prev_;
};


// A flat string reader provides random access to the contents of a
// string independent of the character width of the string.  The handle
// must be valid as long as the reader is being used.
class FlatStringReader : public Relocatable {
 public:
  explicit FlatStringReader(Handle<String> str);
  explicit FlatStringReader(Vector<const char> input);
  void PostGarbageCollection();
  inline uc32 Get(int index);
  int length() { return length_; }
 private:
  String** str_;
  bool is_ascii_;
  int length_;
  const void* start_;
};


// Note that StringInputBuffers are not valid across a GC!  To fix this
// it would have to store a String Handle instead of a String* and
// AsciiStringReadBlock would have to be modified to use memcpy.
//
// StringInputBuffer is able to traverse any string regardless of how
// deeply nested a sequence of ConsStrings it is made of.  However,
// performance will be better if deep strings are flattened before they
// are traversed.  Since flattening requires memory allocation this is
// not always desirable, however (esp. in debugging situations).
class StringInputBuffer: public unibrow::InputBuffer<String, String*, 1024> {
 public:
  virtual void Seek(unsigned pos);
  inline StringInputBuffer(): unibrow::InputBuffer<String, String*, 1024>() {}
  inline StringInputBuffer(String* backing):
      unibrow::InputBuffer<String, String*, 1024>(backing) {}
};


class SafeStringInputBuffer
  : public unibrow::InputBuffer<String, String**, 256> {
 public:
  virtual void Seek(unsigned pos);
  inline SafeStringInputBuffer()
      : unibrow::InputBuffer<String, String**, 256>() {}
  inline SafeStringInputBuffer(String** backing)
      : unibrow::InputBuffer<String, String**, 256>(backing) {}
};


template <typename T>
class VectorIterator {
 public:
  VectorIterator(T* d, int l) : data_(Vector<const T>(d, l)), index_(0) { }
  explicit VectorIterator(Vector<const T> data) : data_(data), index_(0) { }
  T GetNext() { return data_[index_++]; }
  bool has_more() { return index_ < data_.length(); }
 private:
  Vector<const T> data_;
  int index_;
};


// The Oddball describes objects null, undefined, true, and false.
class Oddball: public HeapObject {
 public:
  // [to_string]: Cached to_string computed at startup.
  DECL_ACCESSORS(to_string, String)

  // [to_number]: Cached to_number computed at startup.
  DECL_ACCESSORS(to_number, Object)

  // Casting.
  static inline Oddball* cast(Object* obj);

  // Dispatched behavior.
  void OddballIterateBody(ObjectVisitor* v);
#ifdef DEBUG
  void OddballVerify();
#endif

  // Initialize the fields.
  Object* Initialize(const char* to_string, Object* to_number);

  // Layout description.
  static const int kToStringOffset = HeapObject::kHeaderSize;
  static const int kToNumberOffset = kToStringOffset + kPointerSize;
  static const int kSize = kToNumberOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Oddball);
};


class JSGlobalPropertyCell: public HeapObject {
 public:
  // [value]: value of the global property.
  DECL_ACCESSORS(value, Object)

  // Casting.
  static inline JSGlobalPropertyCell* cast(Object* obj);

  // Dispatched behavior.
  void JSGlobalPropertyCellIterateBody(ObjectVisitor* v);
#ifdef DEBUG
  void JSGlobalPropertyCellVerify();
  void JSGlobalPropertyCellPrint();
#endif

  // Layout description.
  static const int kValueOffset = HeapObject::kHeaderSize;
  static const int kSize = kValueOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSGlobalPropertyCell);
};



// Proxy describes objects pointing from JavaScript to C structures.
// Since they cannot contain references to JS HeapObjects they can be
// placed in old_data_space.
class Proxy: public HeapObject {
 public:
  // [proxy]: field containing the address.
  inline Address proxy();
  inline void set_proxy(Address value);

  // Casting.
  static inline Proxy* cast(Object* obj);

  // Dispatched behavior.
  inline void ProxyIterateBody(ObjectVisitor* v);
#ifdef DEBUG
  void ProxyPrint();
  void ProxyVerify();
#endif

  // Layout description.

  static const int kProxyOffset = HeapObject::kHeaderSize;
  static const int kSize = kProxyOffset + kPointerSize;

  STATIC_CHECK(kProxyOffset == Internals::kProxyProxyOffset);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Proxy);
};


// The JSArray describes JavaScript Arrays
//  Such an array can be in one of two modes:
//    - fast, backing storage is a FixedArray and length <= elements.length();
//       Please note: push and pop can be used to grow and shrink the array.
//    - slow, backing storage is a HashTable with numbers as keys.
class JSArray: public JSObject {
 public:
  // [length]: The length property.
  DECL_ACCESSORS(length, Object)

  Object* JSArrayUpdateLengthFromIndex(uint32_t index, Object* value);

  // Initialize the array with the given capacity. The function may
  // fail due to out-of-memory situations, but only if the requested
  // capacity is non-zero.
  Object* Initialize(int capacity);

  // Set the content of the array to the content of storage.
  inline void SetContent(FixedArray* storage);

  // Casting.
  static inline JSArray* cast(Object* obj);

  // Uses handles.  Ensures that the fixed array backing the JSArray has at
  // least the stated size.
  inline void EnsureSize(int minimum_size_of_backing_fixed_array);

  // Dispatched behavior.
#ifdef DEBUG
  void JSArrayPrint();
  void JSArrayVerify();
#endif

  // Number of element slots to pre-allocate for an empty array.
  static const int kPreallocatedArrayElements = 4;

  // Layout description.
  static const int kLengthOffset = JSObject::kHeaderSize;
  static const int kSize = kLengthOffset + kPointerSize;

 private:
  // Expand the fixed array backing of a fast-case JSArray to at least
  // the requested size.
  void Expand(int minimum_size_of_backing_fixed_array);

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArray);
};


// An accessor must have a getter, but can have no setter.
//
// When setting a property, V8 searches accessors in prototypes.
// If an accessor was found and it does not have a setter,
// the request is ignored.
//
// If the accessor in the prototype has the READ_ONLY property attribute, then
// a new value is added to the local object when the property is set.
// This shadows the accessor in the prototype.
class AccessorInfo: public Struct {
 public:
  DECL_ACCESSORS(getter, Object)
  DECL_ACCESSORS(setter, Object)
  DECL_ACCESSORS(data, Object)
  DECL_ACCESSORS(name, Object)
  DECL_ACCESSORS(flag, Smi)
  DECL_ACCESSORS(load_stub_cache, Object)

  inline bool all_can_read();
  inline void set_all_can_read(bool value);

  inline bool all_can_write();
  inline void set_all_can_write(bool value);

  inline bool prohibits_overwriting();
  inline void set_prohibits_overwriting(bool value);

  inline PropertyAttributes property_attributes();
  inline void set_property_attributes(PropertyAttributes attributes);

  static inline AccessorInfo* cast(Object* obj);

#ifdef DEBUG
  void AccessorInfoPrint();
  void AccessorInfoVerify();
#endif

  static const int kGetterOffset = HeapObject::kHeaderSize;
  static const int kSetterOffset = kGetterOffset + kPointerSize;
  static const int kDataOffset = kSetterOffset + kPointerSize;
  static const int kNameOffset = kDataOffset + kPointerSize;
  static const int kFlagOffset = kNameOffset + kPointerSize;
  static const int kLoadStubCacheOffset = kFlagOffset + kPointerSize;
  static const int kSize = kLoadStubCacheOffset + kPointerSize;

 private:
  // Bit positions in flag.
  static const int kAllCanReadBit = 0;
  static const int kAllCanWriteBit = 1;
  static const int kProhibitsOverwritingBit = 2;
  class AttributesField: public BitField<PropertyAttributes, 3, 3> {};

  DISALLOW_IMPLICIT_CONSTRUCTORS(AccessorInfo);
};


class AccessCheckInfo: public Struct {
 public:
  DECL_ACCESSORS(named_callback, Object)
  DECL_ACCESSORS(indexed_callback, Object)
  DECL_ACCESSORS(data, Object)

  static inline AccessCheckInfo* cast(Object* obj);

#ifdef DEBUG
  void AccessCheckInfoPrint();
  void AccessCheckInfoVerify();
#endif

  static const int kNamedCallbackOffset   = HeapObject::kHeaderSize;
  static const int kIndexedCallbackOffset = kNamedCallbackOffset + kPointerSize;
  static const int kDataOffset = kIndexedCallbackOffset + kPointerSize;
  static const int kSize = kDataOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AccessCheckInfo);
};


class InterceptorInfo: public Struct {
 public:
  DECL_ACCESSORS(getter, Object)
  DECL_ACCESSORS(setter, Object)
  DECL_ACCESSORS(query, Object)
  DECL_ACCESSORS(deleter, Object)
  DECL_ACCESSORS(enumerator, Object)
  DECL_ACCESSORS(data, Object)

  static inline InterceptorInfo* cast(Object* obj);

#ifdef DEBUG
  void InterceptorInfoPrint();
  void InterceptorInfoVerify();
#endif

  static const int kGetterOffset = HeapObject::kHeaderSize;
  static const int kSetterOffset = kGetterOffset + kPointerSize;
  static const int kQueryOffset = kSetterOffset + kPointerSize;
  static const int kDeleterOffset = kQueryOffset + kPointerSize;
  static const int kEnumeratorOffset = kDeleterOffset + kPointerSize;
  static const int kDataOffset = kEnumeratorOffset + kPointerSize;
  static const int kSize = kDataOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InterceptorInfo);
};


class CallHandlerInfo: public Struct {
 public:
  DECL_ACCESSORS(callback, Object)
  DECL_ACCESSORS(data, Object)

  static inline CallHandlerInfo* cast(Object* obj);

#ifdef DEBUG
  void CallHandlerInfoPrint();
  void CallHandlerInfoVerify();
#endif

  static const int kCallbackOffset = HeapObject::kHeaderSize;
  static const int kDataOffset = kCallbackOffset + kPointerSize;
  static const int kSize = kDataOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CallHandlerInfo);
};


class TemplateInfo: public Struct {
 public:
  DECL_ACCESSORS(tag, Object)
  DECL_ACCESSORS(property_list, Object)

#ifdef DEBUG
  void TemplateInfoVerify();
#endif

  static const int kTagOffset          = HeapObject::kHeaderSize;
  static const int kPropertyListOffset = kTagOffset + kPointerSize;
  static const int kHeaderSize         = kPropertyListOffset + kPointerSize;
 protected:
  friend class AGCCVersionRequiresThisClassToHaveAFriendSoHereItIs;
  DISALLOW_IMPLICIT_CONSTRUCTORS(TemplateInfo);
};


class FunctionTemplateInfo: public TemplateInfo {
 public:
  DECL_ACCESSORS(serial_number, Object)
  DECL_ACCESSORS(call_code, Object)
  DECL_ACCESSORS(property_accessors, Object)
  DECL_ACCESSORS(prototype_template, Object)
  DECL_ACCESSORS(parent_template, Object)
  DECL_ACCESSORS(named_property_handler, Object)
  DECL_ACCESSORS(indexed_property_handler, Object)
  DECL_ACCESSORS(instance_template, Object)
  DECL_ACCESSORS(class_name, Object)
  DECL_ACCESSORS(signature, Object)
  DECL_ACCESSORS(instance_call_handler, Object)
  DECL_ACCESSORS(access_check_info, Object)
  DECL_ACCESSORS(flag, Smi)

  // Following properties use flag bits.
  DECL_BOOLEAN_ACCESSORS(hidden_prototype)
  DECL_BOOLEAN_ACCESSORS(undetectable)
  // If the bit is set, object instances created by this function
  // requires access check.
  DECL_BOOLEAN_ACCESSORS(needs_access_check)

  static inline FunctionTemplateInfo* cast(Object* obj);

#ifdef DEBUG
  void FunctionTemplateInfoPrint();
  void FunctionTemplateInfoVerify();
#endif

  static const int kSerialNumberOffset = TemplateInfo::kHeaderSize;
  static const int kCallCodeOffset = kSerialNumberOffset + kPointerSize;
  static const int kPropertyAccessorsOffset = kCallCodeOffset + kPointerSize;
  static const int kPrototypeTemplateOffset =
      kPropertyAccessorsOffset + kPointerSize;
  static const int kParentTemplateOffset =
      kPrototypeTemplateOffset + kPointerSize;
  static const int kNamedPropertyHandlerOffset =
      kParentTemplateOffset + kPointerSize;
  static const int kIndexedPropertyHandlerOffset =
      kNamedPropertyHandlerOffset + kPointerSize;
  static const int kInstanceTemplateOffset =
      kIndexedPropertyHandlerOffset + kPointerSize;
  static const int kClassNameOffset = kInstanceTemplateOffset + kPointerSize;
  static const int kSignatureOffset = kClassNameOffset + kPointerSize;
  static const int kInstanceCallHandlerOffset = kSignatureOffset + kPointerSize;
  static const int kAccessCheckInfoOffset =
      kInstanceCallHandlerOffset + kPointerSize;
  static const int kFlagOffset = kAccessCheckInfoOffset + kPointerSize;
  static const int kSize = kFlagOffset + kPointerSize;

 private:
  // Bit position in the flag, from least significant bit position.
  static const int kHiddenPrototypeBit   = 0;
  static const int kUndetectableBit      = 1;
  static const int kNeedsAccessCheckBit  = 2;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionTemplateInfo);
};


class ObjectTemplateInfo: public TemplateInfo {
 public:
  DECL_ACCESSORS(constructor, Object)
  DECL_ACCESSORS(internal_field_count, Object)

  static inline ObjectTemplateInfo* cast(Object* obj);

#ifdef DEBUG
  void ObjectTemplateInfoPrint();
  void ObjectTemplateInfoVerify();
#endif

  static const int kConstructorOffset = TemplateInfo::kHeaderSize;
  static const int kInternalFieldCountOffset =
      kConstructorOffset + kPointerSize;
  static const int kSize = kInternalFieldCountOffset + kPointerSize;
};


class SignatureInfo: public Struct {
 public:
  DECL_ACCESSORS(receiver, Object)
  DECL_ACCESSORS(args, Object)

  static inline SignatureInfo* cast(Object* obj);

#ifdef DEBUG
  void SignatureInfoPrint();
  void SignatureInfoVerify();
#endif

  static const int kReceiverOffset = Struct::kHeaderSize;
  static const int kArgsOffset     = kReceiverOffset + kPointerSize;
  static const int kSize           = kArgsOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SignatureInfo);
};


class TypeSwitchInfo: public Struct {
 public:
  DECL_ACCESSORS(types, Object)

  static inline TypeSwitchInfo* cast(Object* obj);

#ifdef DEBUG
  void TypeSwitchInfoPrint();
  void TypeSwitchInfoVerify();
#endif

  static const int kTypesOffset = Struct::kHeaderSize;
  static const int kSize        = kTypesOffset + kPointerSize;
};


#ifdef ENABLE_DEBUGGER_SUPPORT
// The DebugInfo class holds additional information for a function being
// debugged.
class DebugInfo: public Struct {
 public:
  // The shared function info for the source being debugged.
  DECL_ACCESSORS(shared, SharedFunctionInfo)
  // Code object for the original code.
  DECL_ACCESSORS(original_code, Code)
  // Code object for the patched code. This code object is the code object
  // currently active for the function.
  DECL_ACCESSORS(code, Code)
  // Fixed array holding status information for each active break point.
  DECL_ACCESSORS(break_points, FixedArray)

  // Check if there is a break point at a code position.
  bool HasBreakPoint(int code_position);
  // Get the break point info object for a code position.
  Object* GetBreakPointInfo(int code_position);
  // Clear a break point.
  static void ClearBreakPoint(Handle<DebugInfo> debug_info,
                              int code_position,
                              Handle<Object> break_point_object);
  // Set a break point.
  static void SetBreakPoint(Handle<DebugInfo> debug_info, int code_position,
                            int source_position, int statement_position,
                            Handle<Object> break_point_object);
  // Get the break point objects for a code position.
  Object* GetBreakPointObjects(int code_position);
  // Find the break point info holding this break point object.
  static Object* FindBreakPointInfo(Handle<DebugInfo> debug_info,
                                    Handle<Object> break_point_object);
  // Get the number of break points for this function.
  int GetBreakPointCount();

  static inline DebugInfo* cast(Object* obj);

#ifdef DEBUG
  void DebugInfoPrint();
  void DebugInfoVerify();
#endif

  static const int kSharedFunctionInfoIndex = Struct::kHeaderSize;
  static const int kOriginalCodeIndex = kSharedFunctionInfoIndex + kPointerSize;
  static const int kPatchedCodeIndex = kOriginalCodeIndex + kPointerSize;
  static const int kActiveBreakPointsCountIndex =
      kPatchedCodeIndex + kPointerSize;
  static const int kBreakPointsStateIndex =
      kActiveBreakPointsCountIndex + kPointerSize;
  static const int kSize = kBreakPointsStateIndex + kPointerSize;

 private:
  static const int kNoBreakPointInfo = -1;

  // Lookup the index in the break_points array for a code position.
  int GetBreakPointInfoIndex(int code_position);

  DISALLOW_IMPLICIT_CONSTRUCTORS(DebugInfo);
};


// The BreakPointInfo class holds information for break points set in a
// function. The DebugInfo object holds a BreakPointInfo object for each code
// position with one or more break points.
class BreakPointInfo: public Struct {
 public:
  // The position in the code for the break point.
  DECL_ACCESSORS(code_position, Smi)
  // The position in the source for the break position.
  DECL_ACCESSORS(source_position, Smi)
  // The position in the source for the last statement before this break
  // position.
  DECL_ACCESSORS(statement_position, Smi)
  // List of related JavaScript break points.
  DECL_ACCESSORS(break_point_objects, Object)

  // Removes a break point.
  static void ClearBreakPoint(Handle<BreakPointInfo> info,
                              Handle<Object> break_point_object);
  // Set a break point.
  static void SetBreakPoint(Handle<BreakPointInfo> info,
                            Handle<Object> break_point_object);
  // Check if break point info has this break point object.
  static bool HasBreakPointObject(Handle<BreakPointInfo> info,
                                  Handle<Object> break_point_object);
  // Get the number of break points for this code position.
  int GetBreakPointCount();

  static inline BreakPointInfo* cast(Object* obj);

#ifdef DEBUG
  void BreakPointInfoPrint();
  void BreakPointInfoVerify();
#endif

  static const int kCodePositionIndex = Struct::kHeaderSize;
  static const int kSourcePositionIndex = kCodePositionIndex + kPointerSize;
  static const int kStatementPositionIndex =
      kSourcePositionIndex + kPointerSize;
  static const int kBreakPointObjectsIndex =
      kStatementPositionIndex + kPointerSize;
  static const int kSize = kBreakPointObjectsIndex + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BreakPointInfo);
};
#endif  // ENABLE_DEBUGGER_SUPPORT


#undef DECL_BOOLEAN_ACCESSORS
#undef DECL_ACCESSORS


// Abstract base class for visiting, and optionally modifying, the
// pointers contained in Objects. Used in GC and serialization/deserialization.
class ObjectVisitor BASE_EMBEDDED {
 public:
  virtual ~ObjectVisitor() {}

  // Visits a contiguous arrays of pointers in the half-open range
  // [start, end). Any or all of the values may be modified on return.
  virtual void VisitPointers(Object** start, Object** end) = 0;

  // To allow lazy clearing of inline caches the visitor has
  // a rich interface for iterating over Code objects..

  // Visits a code target in the instruction stream.
  virtual void VisitCodeTarget(RelocInfo* rinfo);

  // Visits a runtime entry in the instruction stream.
  virtual void VisitRuntimeEntry(RelocInfo* rinfo) {}

  // Visits the resource of an ASCII or two-byte string.
  virtual void VisitExternalAsciiString(
      v8::String::ExternalAsciiStringResource** resource) {}
  virtual void VisitExternalTwoByteString(
      v8::String::ExternalStringResource** resource) {}

  // Visits a debug call target in the instruction stream.
  virtual void VisitDebugTarget(RelocInfo* rinfo);

  // Handy shorthand for visiting a single pointer.
  virtual void VisitPointer(Object** p) { VisitPointers(p, p + 1); }

  // Visits a contiguous arrays of external references (references to the C++
  // heap) in the half-open range [start, end). Any or all of the values
  // may be modified on return.
  virtual void VisitExternalReferences(Address* start, Address* end) {}

  inline void VisitExternalReference(Address* p) {
    VisitExternalReferences(p, p + 1);
  }

#ifdef DEBUG
  // Intended for serialization/deserialization checking: insert, or
  // check for the presence of, a tag at this position in the stream.
  virtual void Synchronize(const char* tag) {}
#else
  inline void Synchronize(const char* tag) {}
#endif
};


// BooleanBit is a helper class for setting and getting a bit in an
// integer or Smi.
class BooleanBit : public AllStatic {
 public:
  static inline bool get(Smi* smi, int bit_position) {
    return get(smi->value(), bit_position);
  }

  static inline bool get(int value, int bit_position) {
    return (value & (1 << bit_position)) != 0;
  }

  static inline Smi* set(Smi* smi, int bit_position, bool v) {
    return Smi::FromInt(set(smi->value(), bit_position, v));
  }

  static inline int set(int value, int bit_position, bool v) {
    if (v) {
      value |= (1 << bit_position);
    } else {
      value &= ~(1 << bit_position);
    }
    return value;
  }
};

} }  // namespace v8::internal

#endif  // V8_OBJECTS_H_

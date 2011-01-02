// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "disassembler.h"
#include "disasm.h"
#include "jsregexp.h"
#include "objects-visiting.h"

namespace v8 {
namespace internal {

#ifdef OBJECT_PRINT

static const char* TypeToString(InstanceType type);


void MaybeObject::Print(FILE* out) {
  Object* this_as_object;
  if (ToObject(&this_as_object)) {
    if (this_as_object->IsSmi()) {
      Smi::cast(this_as_object)->SmiPrint(out);
    } else {
      HeapObject::cast(this_as_object)->HeapObjectPrint(out);
    }
  } else {
    Failure::cast(this)->FailurePrint(out);
  }
  Flush(out);
}


void MaybeObject::PrintLn(FILE* out) {
  Print(out);
  PrintF(out, "\n");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void MaybeObject::Verify() {
  Object* this_as_object;
  if (ToObject(&this_as_object)) {
    if (this_as_object->IsSmi()) {
      Smi::cast(this_as_object)->SmiVerify();
    } else {
      HeapObject::cast(this_as_object)->HeapObjectVerify();
    }
  } else {
    Failure::cast(this)->FailureVerify();
  }
}


void Object::VerifyPointer(Object* p) {
  if (p->IsHeapObject()) {
    HeapObject::VerifyHeapPointer(p);
  } else {
    ASSERT(p->IsSmi());
  }
}


void Smi::SmiVerify() {
  ASSERT(IsSmi());
}


void Failure::FailureVerify() {
  ASSERT(IsFailure());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void HeapObject::PrintHeader(FILE* out, const char* id) {
  PrintF(out, "%p: [%s]\n", reinterpret_cast<void*>(this), id);
}


void HeapObject::HeapObjectPrint(FILE* out) {
  InstanceType instance_type = map()->instance_type();

  HandleScope scope;
  if (instance_type < FIRST_NONSTRING_TYPE) {
    String::cast(this)->StringPrint(out);
    return;
  }

  switch (instance_type) {
    case MAP_TYPE:
      Map::cast(this)->MapPrint(out);
      break;
    case HEAP_NUMBER_TYPE:
      HeapNumber::cast(this)->HeapNumberPrint(out);
      break;
    case FIXED_ARRAY_TYPE:
      FixedArray::cast(this)->FixedArrayPrint(out);
      break;
    case BYTE_ARRAY_TYPE:
      ByteArray::cast(this)->ByteArrayPrint(out);
      break;
    case PIXEL_ARRAY_TYPE:
      PixelArray::cast(this)->PixelArrayPrint(out);
      break;
    case EXTERNAL_BYTE_ARRAY_TYPE:
      ExternalByteArray::cast(this)->ExternalByteArrayPrint(out);
      break;
    case EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
      ExternalUnsignedByteArray::cast(this)
          ->ExternalUnsignedByteArrayPrint(out);
      break;
    case EXTERNAL_SHORT_ARRAY_TYPE:
      ExternalShortArray::cast(this)->ExternalShortArrayPrint(out);
      break;
    case EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
      ExternalUnsignedShortArray::cast(this)
          ->ExternalUnsignedShortArrayPrint(out);
      break;
    case EXTERNAL_INT_ARRAY_TYPE:
      ExternalIntArray::cast(this)->ExternalIntArrayPrint(out);
      break;
    case EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
      ExternalUnsignedIntArray::cast(this)->ExternalUnsignedIntArrayPrint(out);
      break;
    case EXTERNAL_FLOAT_ARRAY_TYPE:
      ExternalFloatArray::cast(this)->ExternalFloatArrayPrint(out);
      break;
    case FILLER_TYPE:
      PrintF(out, "filler");
      break;
    case JS_OBJECT_TYPE:  // fall through
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_ARRAY_TYPE:
    case JS_REGEXP_TYPE:
      JSObject::cast(this)->JSObjectPrint(out);
      break;
    case ODDBALL_TYPE:
      Oddball::cast(this)->to_string()->Print(out);
      break;
    case JS_FUNCTION_TYPE:
      JSFunction::cast(this)->JSFunctionPrint(out);
      break;
    case JS_GLOBAL_PROXY_TYPE:
      JSGlobalProxy::cast(this)->JSGlobalProxyPrint(out);
      break;
    case JS_GLOBAL_OBJECT_TYPE:
      JSGlobalObject::cast(this)->JSGlobalObjectPrint(out);
      break;
    case JS_BUILTINS_OBJECT_TYPE:
      JSBuiltinsObject::cast(this)->JSBuiltinsObjectPrint(out);
      break;
    case JS_VALUE_TYPE:
      PrintF(out, "Value wrapper around:");
      JSValue::cast(this)->value()->Print(out);
      break;
    case CODE_TYPE:
      Code::cast(this)->CodePrint(out);
      break;
    case PROXY_TYPE:
      Proxy::cast(this)->ProxyPrint(out);
      break;
    case SHARED_FUNCTION_INFO_TYPE:
      SharedFunctionInfo::cast(this)->SharedFunctionInfoPrint(out);
      break;
    case JS_GLOBAL_PROPERTY_CELL_TYPE:
      JSGlobalPropertyCell::cast(this)->JSGlobalPropertyCellPrint(out);
      break;
#define MAKE_STRUCT_CASE(NAME, Name, name) \
  case NAME##_TYPE:                        \
    Name::cast(this)->Name##Print(out);    \
    break;
  STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE

    default:
      PrintF(out, "UNKNOWN TYPE %d", map()->instance_type());
      UNREACHABLE();
      break;
  }
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void HeapObject::HeapObjectVerify() {
  InstanceType instance_type = map()->instance_type();

  if (instance_type < FIRST_NONSTRING_TYPE) {
    String::cast(this)->StringVerify();
    return;
  }

  switch (instance_type) {
    case MAP_TYPE:
      Map::cast(this)->MapVerify();
      break;
    case HEAP_NUMBER_TYPE:
      HeapNumber::cast(this)->HeapNumberVerify();
      break;
    case FIXED_ARRAY_TYPE:
      FixedArray::cast(this)->FixedArrayVerify();
      break;
    case BYTE_ARRAY_TYPE:
      ByteArray::cast(this)->ByteArrayVerify();
      break;
    case PIXEL_ARRAY_TYPE:
      PixelArray::cast(this)->PixelArrayVerify();
      break;
    case EXTERNAL_BYTE_ARRAY_TYPE:
      ExternalByteArray::cast(this)->ExternalByteArrayVerify();
      break;
    case EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
      ExternalUnsignedByteArray::cast(this)->ExternalUnsignedByteArrayVerify();
      break;
    case EXTERNAL_SHORT_ARRAY_TYPE:
      ExternalShortArray::cast(this)->ExternalShortArrayVerify();
      break;
    case EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
      ExternalUnsignedShortArray::cast(this)->
          ExternalUnsignedShortArrayVerify();
      break;
    case EXTERNAL_INT_ARRAY_TYPE:
      ExternalIntArray::cast(this)->ExternalIntArrayVerify();
      break;
    case EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
      ExternalUnsignedIntArray::cast(this)->ExternalUnsignedIntArrayVerify();
      break;
    case EXTERNAL_FLOAT_ARRAY_TYPE:
      ExternalFloatArray::cast(this)->ExternalFloatArrayVerify();
      break;
    case CODE_TYPE:
      Code::cast(this)->CodeVerify();
      break;
    case ODDBALL_TYPE:
      Oddball::cast(this)->OddballVerify();
      break;
    case JS_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
      JSObject::cast(this)->JSObjectVerify();
      break;
    case JS_VALUE_TYPE:
      JSValue::cast(this)->JSValueVerify();
      break;
    case JS_FUNCTION_TYPE:
      JSFunction::cast(this)->JSFunctionVerify();
      break;
    case JS_GLOBAL_PROXY_TYPE:
      JSGlobalProxy::cast(this)->JSGlobalProxyVerify();
      break;
    case JS_GLOBAL_OBJECT_TYPE:
      JSGlobalObject::cast(this)->JSGlobalObjectVerify();
      break;
    case JS_BUILTINS_OBJECT_TYPE:
      JSBuiltinsObject::cast(this)->JSBuiltinsObjectVerify();
      break;
    case JS_GLOBAL_PROPERTY_CELL_TYPE:
      JSGlobalPropertyCell::cast(this)->JSGlobalPropertyCellVerify();
      break;
    case JS_ARRAY_TYPE:
      JSArray::cast(this)->JSArrayVerify();
      break;
    case JS_REGEXP_TYPE:
      JSRegExp::cast(this)->JSRegExpVerify();
      break;
    case FILLER_TYPE:
      break;
    case PROXY_TYPE:
      Proxy::cast(this)->ProxyVerify();
      break;
    case SHARED_FUNCTION_INFO_TYPE:
      SharedFunctionInfo::cast(this)->SharedFunctionInfoVerify();
      break;

#define MAKE_STRUCT_CASE(NAME, Name, name) \
  case NAME##_TYPE:                        \
    Name::cast(this)->Name##Verify();      \
    break;
    STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE

    default:
      UNREACHABLE();
      break;
  }
}


void HeapObject::VerifyHeapPointer(Object* p) {
  ASSERT(p->IsHeapObject());
  ASSERT(Heap::Contains(HeapObject::cast(p)));
}


void HeapNumber::HeapNumberVerify() {
  ASSERT(IsHeapNumber());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void ByteArray::ByteArrayPrint(FILE* out) {
  PrintF(out, "byte array, data starts at %p", GetDataStartAddress());
}


void PixelArray::PixelArrayPrint(FILE* out) {
  PrintF(out, "pixel array");
}


void ExternalByteArray::ExternalByteArrayPrint(FILE* out) {
  PrintF(out, "external byte array");
}


void ExternalUnsignedByteArray::ExternalUnsignedByteArrayPrint(FILE* out) {
  PrintF(out, "external unsigned byte array");
}


void ExternalShortArray::ExternalShortArrayPrint(FILE* out) {
  PrintF(out, "external short array");
}


void ExternalUnsignedShortArray::ExternalUnsignedShortArrayPrint(FILE* out) {
  PrintF(out, "external unsigned short array");
}


void ExternalIntArray::ExternalIntArrayPrint(FILE* out) {
  PrintF(out, "external int array");
}


void ExternalUnsignedIntArray::ExternalUnsignedIntArrayPrint(FILE* out) {
  PrintF(out, "external unsigned int array");
}


void ExternalFloatArray::ExternalFloatArrayPrint(FILE* out) {
  PrintF(out, "external float array");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void ByteArray::ByteArrayVerify() {
  ASSERT(IsByteArray());
}


void PixelArray::PixelArrayVerify() {
  ASSERT(IsPixelArray());
}


void ExternalByteArray::ExternalByteArrayVerify() {
  ASSERT(IsExternalByteArray());
}


void ExternalUnsignedByteArray::ExternalUnsignedByteArrayVerify() {
  ASSERT(IsExternalUnsignedByteArray());
}


void ExternalShortArray::ExternalShortArrayVerify() {
  ASSERT(IsExternalShortArray());
}


void ExternalUnsignedShortArray::ExternalUnsignedShortArrayVerify() {
  ASSERT(IsExternalUnsignedShortArray());
}


void ExternalIntArray::ExternalIntArrayVerify() {
  ASSERT(IsExternalIntArray());
}


void ExternalUnsignedIntArray::ExternalUnsignedIntArrayVerify() {
  ASSERT(IsExternalUnsignedIntArray());
}


void ExternalFloatArray::ExternalFloatArrayVerify() {
  ASSERT(IsExternalFloatArray());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void JSObject::PrintProperties(FILE* out) {
  if (HasFastProperties()) {
    DescriptorArray* descs = map()->instance_descriptors();
    for (int i = 0; i < descs->number_of_descriptors(); i++) {
      PrintF(out, "   ");
      descs->GetKey(i)->StringPrint(out);
      PrintF(out, ": ");
      switch (descs->GetType(i)) {
        case FIELD: {
          int index = descs->GetFieldIndex(i);
          FastPropertyAt(index)->ShortPrint(out);
          PrintF(out, " (field at offset %d)\n", index);
          break;
        }
        case CONSTANT_FUNCTION:
          descs->GetConstantFunction(i)->ShortPrint(out);
          PrintF(out, " (constant function)\n");
          break;
        case CALLBACKS:
          descs->GetCallbacksObject(i)->ShortPrint(out);
          PrintF(out, " (callback)\n");
          break;
        case MAP_TRANSITION:
          PrintF(out, " (map transition)\n");
          break;
        case CONSTANT_TRANSITION:
          PrintF(out, " (constant transition)\n");
          break;
        case NULL_DESCRIPTOR:
          PrintF(out, " (null descriptor)\n");
          break;
        default:
          UNREACHABLE();
          break;
      }
    }
  } else {
    property_dictionary()->Print(out);
  }
}


void JSObject::PrintElements(FILE* out) {
  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      // Print in array notation for non-sparse arrays.
      FixedArray* p = FixedArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: ", i);
        p->get(i)->ShortPrint(out);
        PrintF(out, "\n");
      }
      break;
    }
    case PIXEL_ELEMENTS: {
      PixelArray* p = PixelArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, p->get(i));
      }
      break;
    }
    case EXTERNAL_BYTE_ELEMENTS: {
      ExternalByteArray* p = ExternalByteArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, static_cast<int>(p->get(i)));
      }
      break;
    }
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS: {
      ExternalUnsignedByteArray* p =
          ExternalUnsignedByteArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, static_cast<int>(p->get(i)));
      }
      break;
    }
    case EXTERNAL_SHORT_ELEMENTS: {
      ExternalShortArray* p = ExternalShortArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, static_cast<int>(p->get(i)));
      }
      break;
    }
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS: {
      ExternalUnsignedShortArray* p =
          ExternalUnsignedShortArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, static_cast<int>(p->get(i)));
      }
      break;
    }
    case EXTERNAL_INT_ELEMENTS: {
      ExternalIntArray* p = ExternalIntArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, static_cast<int>(p->get(i)));
      }
      break;
    }
    case EXTERNAL_UNSIGNED_INT_ELEMENTS: {
      ExternalUnsignedIntArray* p =
          ExternalUnsignedIntArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, static_cast<int>(p->get(i)));
      }
      break;
    }
    case EXTERNAL_FLOAT_ELEMENTS: {
      ExternalFloatArray* p = ExternalFloatArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %f\n", i, p->get(i));
      }
      break;
    }
    case DICTIONARY_ELEMENTS:
      elements()->Print(out);
      break;
    default:
      UNREACHABLE();
      break;
  }
}


void JSObject::JSObjectPrint(FILE* out) {
  PrintF(out, "%p: [JSObject]\n", reinterpret_cast<void*>(this));
  PrintF(out, " - map = %p\n", reinterpret_cast<void*>(map()));
  PrintF(out, " - prototype = %p\n", reinterpret_cast<void*>(GetPrototype()));
  PrintF(out, " {\n");
  PrintProperties(out);
  PrintElements(out);
  PrintF(out, " }\n");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void JSObject::JSObjectVerify() {
  VerifyHeapPointer(properties());
  VerifyHeapPointer(elements());
  if (HasFastProperties()) {
    CHECK_EQ(map()->unused_property_fields(),
             (map()->inobject_properties() + properties()->length() -
              map()->NextFreePropertyIndex()));
  }
  ASSERT(map()->has_fast_elements() ==
         (elements()->map() == Heap::fixed_array_map() ||
          elements()->map() == Heap::fixed_cow_array_map()));
  ASSERT(map()->has_fast_elements() == HasFastElements());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
static const char* TypeToString(InstanceType type) {
  switch (type) {
    case INVALID_TYPE: return "INVALID";
    case MAP_TYPE: return "MAP";
    case HEAP_NUMBER_TYPE: return "HEAP_NUMBER";
    case SYMBOL_TYPE: return "SYMBOL";
    case ASCII_SYMBOL_TYPE: return "ASCII_SYMBOL";
    case CONS_SYMBOL_TYPE: return "CONS_SYMBOL";
    case CONS_ASCII_SYMBOL_TYPE: return "CONS_ASCII_SYMBOL";
    case EXTERNAL_ASCII_SYMBOL_TYPE:
    case EXTERNAL_SYMBOL_WITH_ASCII_DATA_TYPE:
    case EXTERNAL_SYMBOL_TYPE: return "EXTERNAL_SYMBOL";
    case ASCII_STRING_TYPE: return "ASCII_STRING";
    case STRING_TYPE: return "TWO_BYTE_STRING";
    case CONS_STRING_TYPE:
    case CONS_ASCII_STRING_TYPE: return "CONS_STRING";
    case EXTERNAL_ASCII_STRING_TYPE:
    case EXTERNAL_STRING_WITH_ASCII_DATA_TYPE:
    case EXTERNAL_STRING_TYPE: return "EXTERNAL_STRING";
    case FIXED_ARRAY_TYPE: return "FIXED_ARRAY";
    case BYTE_ARRAY_TYPE: return "BYTE_ARRAY";
    case PIXEL_ARRAY_TYPE: return "PIXEL_ARRAY";
    case EXTERNAL_BYTE_ARRAY_TYPE: return "EXTERNAL_BYTE_ARRAY";
    case EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
      return "EXTERNAL_UNSIGNED_BYTE_ARRAY";
    case EXTERNAL_SHORT_ARRAY_TYPE: return "EXTERNAL_SHORT_ARRAY";
    case EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
      return "EXTERNAL_UNSIGNED_SHORT_ARRAY";
    case EXTERNAL_INT_ARRAY_TYPE: return "EXTERNAL_INT_ARRAY";
    case EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
      return "EXTERNAL_UNSIGNED_INT_ARRAY";
    case EXTERNAL_FLOAT_ARRAY_TYPE: return "EXTERNAL_FLOAT_ARRAY";
    case FILLER_TYPE: return "FILLER";
    case JS_OBJECT_TYPE: return "JS_OBJECT";
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE: return "JS_CONTEXT_EXTENSION_OBJECT";
    case ODDBALL_TYPE: return "ODDBALL";
    case JS_GLOBAL_PROPERTY_CELL_TYPE: return "JS_GLOBAL_PROPERTY_CELL";
    case SHARED_FUNCTION_INFO_TYPE: return "SHARED_FUNCTION_INFO";
    case JS_FUNCTION_TYPE: return "JS_FUNCTION";
    case CODE_TYPE: return "CODE";
    case JS_ARRAY_TYPE: return "JS_ARRAY";
    case JS_REGEXP_TYPE: return "JS_REGEXP";
    case JS_VALUE_TYPE: return "JS_VALUE";
    case JS_GLOBAL_OBJECT_TYPE: return "JS_GLOBAL_OBJECT";
    case JS_BUILTINS_OBJECT_TYPE: return "JS_BUILTINS_OBJECT";
    case JS_GLOBAL_PROXY_TYPE: return "JS_GLOBAL_PROXY";
    case PROXY_TYPE: return "PROXY";
#define MAKE_STRUCT_CASE(NAME, Name, name) case NAME##_TYPE: return #NAME;
  STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
  }
  return "UNKNOWN";
}


void Map::MapPrint(FILE* out) {
  HeapObject::PrintHeader(out, "Map");
  PrintF(out, " - type: %s\n", TypeToString(instance_type()));
  PrintF(out, " - instance size: %d\n", instance_size());
  PrintF(out, " - inobject properties: %d\n", inobject_properties());
  PrintF(out, " - pre-allocated property fields: %d\n",
      pre_allocated_property_fields());
  PrintF(out, " - unused property fields: %d\n", unused_property_fields());
  if (is_hidden_prototype()) {
    PrintF(out, " - hidden_prototype\n");
  }
  if (has_named_interceptor()) {
    PrintF(out, " - named_interceptor\n");
  }
  if (has_indexed_interceptor()) {
    PrintF(out, " - indexed_interceptor\n");
  }
  if (is_undetectable()) {
    PrintF(out, " - undetectable\n");
  }
  if (has_instance_call_handler()) {
    PrintF(out, " - instance_call_handler\n");
  }
  if (is_access_check_needed()) {
    PrintF(out, " - access_check_needed\n");
  }
  PrintF(out, " - instance descriptors: ");
  instance_descriptors()->ShortPrint(out);
  PrintF(out, "\n - prototype: ");
  prototype()->ShortPrint(out);
  PrintF(out, "\n - constructor: ");
  constructor()->ShortPrint(out);
  PrintF(out, "\n");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void Map::MapVerify() {
  ASSERT(!Heap::InNewSpace(this));
  ASSERT(FIRST_TYPE <= instance_type() && instance_type() <= LAST_TYPE);
  ASSERT(instance_size() == kVariableSizeSentinel ||
         (kPointerSize <= instance_size() &&
          instance_size() < Heap::Capacity()));
  VerifyHeapPointer(prototype());
  VerifyHeapPointer(instance_descriptors());
}


void Map::SharedMapVerify() {
  MapVerify();
  ASSERT(is_shared());
  ASSERT_EQ(Heap::empty_descriptor_array(), instance_descriptors());
  ASSERT_EQ(Heap::empty_fixed_array(), code_cache());
  ASSERT_EQ(0, pre_allocated_property_fields());
  ASSERT_EQ(0, unused_property_fields());
  ASSERT_EQ(StaticVisitorBase::GetVisitorId(instance_type(), instance_size()),
      visitor_id());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void CodeCache::CodeCachePrint(FILE* out) {
  HeapObject::PrintHeader(out, "CodeCache");
  PrintF(out, "\n - default_cache: ");
  default_cache()->ShortPrint(out);
  PrintF(out, "\n - normal_type_cache: ");
  normal_type_cache()->ShortPrint(out);
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void CodeCache::CodeCacheVerify() {
  VerifyHeapPointer(default_cache());
  VerifyHeapPointer(normal_type_cache());
  ASSERT(default_cache()->IsFixedArray());
  ASSERT(normal_type_cache()->IsUndefined()
         || normal_type_cache()->IsCodeCacheHashTable());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void FixedArray::FixedArrayPrint(FILE* out) {
  HeapObject::PrintHeader(out, "FixedArray");
  PrintF(out, " - length: %d", length());
  for (int i = 0; i < length(); i++) {
    PrintF(out, "\n  [%d]: ", i);
    get(i)->ShortPrint(out);
  }
  PrintF(out, "\n");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void FixedArray::FixedArrayVerify() {
  for (int i = 0; i < length(); i++) {
    Object* e = get(i);
    if (e->IsHeapObject()) {
      VerifyHeapPointer(e);
    } else {
      e->Verify();
    }
  }
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void JSValue::JSValuePrint(FILE* out) {
  HeapObject::PrintHeader(out, "ValueObject");
  value()->Print(out);
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void JSValue::JSValueVerify() {
  Object* v = value();
  if (v->IsHeapObject()) {
    VerifyHeapPointer(v);
  }
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void String::StringPrint(FILE* out) {
  if (StringShape(this).IsSymbol()) {
    PrintF(out, "#");
  } else if (StringShape(this).IsCons()) {
    PrintF(out, "c\"");
  } else {
    PrintF(out, "\"");
  }

  const char truncated_epilogue[] = "...<truncated>";
  int len = length();
  if (!FLAG_use_verbose_printer) {
    if (len > 100) {
      len = 100 - sizeof(truncated_epilogue);
    }
  }
  for (int i = 0; i < len; i++) {
    PrintF(out, "%c", Get(i));
  }
  if (len != length()) {
    PrintF(out, "%s", truncated_epilogue);
  }

  if (!StringShape(this).IsSymbol()) PrintF(out, "\"");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void String::StringVerify() {
  CHECK(IsString());
  CHECK(length() >= 0 && length() <= Smi::kMaxValue);
  if (IsSymbol()) {
    CHECK(!Heap::InNewSpace(this));
  }
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void JSFunction::JSFunctionPrint(FILE* out) {
  HeapObject::PrintHeader(out, "Function");
  PrintF(out, " - map = 0x%p\n", reinterpret_cast<void*>(map()));
  PrintF(out, " - initial_map = ");
  if (has_initial_map()) {
    initial_map()->ShortPrint(out);
  }
  PrintF(out, "\n - shared_info = ");
  shared()->ShortPrint(out);
  PrintF(out, "\n   - name = ");
  shared()->name()->Print(out);
  PrintF(out, "\n - context = ");
  unchecked_context()->ShortPrint(out);
  PrintF(out, "\n - code = ");
  code()->ShortPrint(out);
  PrintF(out, "\n");

  PrintProperties(out);
  PrintElements(out);

  PrintF(out, "\n");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void JSFunction::JSFunctionVerify() {
  CHECK(IsJSFunction());
  VerifyObjectField(kPrototypeOrInitialMapOffset);
  VerifyObjectField(kNextFunctionLinkOffset);
  CHECK(next_function_link()->IsUndefined() ||
        next_function_link()->IsJSFunction());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void SharedFunctionInfo::SharedFunctionInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "SharedFunctionInfo");
  PrintF(out, " - name: ");
  name()->ShortPrint(out);
  PrintF(out, "\n - expected_nof_properties: %d", expected_nof_properties());
  PrintF(out, "\n - instance class name = ");
  instance_class_name()->Print(out);
  PrintF(out, "\n - code = ");
  code()->ShortPrint(out);
  PrintF(out, "\n - source code = ");
  GetSourceCode()->ShortPrint(out);
  // Script files are often large, hard to read.
  // PrintF(out, "\n - script =");
  // script()->Print(out);
  PrintF(out, "\n - function token position = %d", function_token_position());
  PrintF(out, "\n - start position = %d", start_position());
  PrintF(out, "\n - end position = %d", end_position());
  PrintF(out, "\n - is expression = %d", is_expression());
  PrintF(out, "\n - debug info = ");
  debug_info()->ShortPrint(out);
  PrintF(out, "\n - length = %d", length());
  PrintF(out, "\n - has_only_simple_this_property_assignments = %d",
         has_only_simple_this_property_assignments());
  PrintF(out, "\n - this_property_assignments = ");
  this_property_assignments()->ShortPrint(out);
  PrintF(out, "\n");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void SharedFunctionInfo::SharedFunctionInfoVerify() {
  CHECK(IsSharedFunctionInfo());
  VerifyObjectField(kNameOffset);
  VerifyObjectField(kCodeOffset);
  VerifyObjectField(kScopeInfoOffset);
  VerifyObjectField(kInstanceClassNameOffset);
  VerifyObjectField(kFunctionDataOffset);
  VerifyObjectField(kScriptOffset);
  VerifyObjectField(kDebugInfoOffset);
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void JSGlobalProxy::JSGlobalProxyPrint(FILE* out) {
  PrintF(out, "global_proxy");
  JSObjectPrint(out);
  PrintF(out, "context : ");
  context()->ShortPrint(out);
  PrintF(out, "\n");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void JSGlobalProxy::JSGlobalProxyVerify() {
  CHECK(IsJSGlobalProxy());
  JSObjectVerify();
  VerifyObjectField(JSGlobalProxy::kContextOffset);
  // Make sure that this object has no properties, elements.
  CHECK_EQ(0, properties()->length());
  CHECK(HasFastElements());
  CHECK_EQ(0, FixedArray::cast(elements())->length());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void JSGlobalObject::JSGlobalObjectPrint(FILE* out) {
  PrintF(out, "global ");
  JSObjectPrint(out);
  PrintF(out, "global context : ");
  global_context()->ShortPrint(out);
  PrintF(out, "\n");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void JSGlobalObject::JSGlobalObjectVerify() {
  CHECK(IsJSGlobalObject());
  JSObjectVerify();
  for (int i = GlobalObject::kBuiltinsOffset;
       i < JSGlobalObject::kSize;
       i += kPointerSize) {
    VerifyObjectField(i);
  }
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void JSBuiltinsObject::JSBuiltinsObjectPrint(FILE* out) {
  PrintF(out, "builtins ");
  JSObjectPrint(out);
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void JSBuiltinsObject::JSBuiltinsObjectVerify() {
  CHECK(IsJSBuiltinsObject());
  JSObjectVerify();
  for (int i = GlobalObject::kBuiltinsOffset;
       i < JSBuiltinsObject::kSize;
       i += kPointerSize) {
    VerifyObjectField(i);
  }
}


void Oddball::OddballVerify() {
  CHECK(IsOddball());
  VerifyHeapPointer(to_string());
  Object* number = to_number();
  if (number->IsHeapObject()) {
    ASSERT(number == Heap::nan_value());
  } else {
    ASSERT(number->IsSmi());
    int value = Smi::cast(number)->value();
    ASSERT(value == 0 || value == 1 || value == -1 ||
           value == -2 || value == -3);
  }
}


void JSGlobalPropertyCell::JSGlobalPropertyCellVerify() {
  CHECK(IsJSGlobalPropertyCell());
  VerifyObjectField(kValueOffset);
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void JSGlobalPropertyCell::JSGlobalPropertyCellPrint(FILE* out) {
  HeapObject::PrintHeader(out, "JSGlobalPropertyCell");
}


void Code::CodePrint(FILE* out) {
  HeapObject::PrintHeader(out, "Code");
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_use_verbose_printer) {
    Disassemble(NULL, out);
  }
#endif
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void Code::CodeVerify() {
  CHECK(IsAligned(reinterpret_cast<intptr_t>(instruction_start()),
                  kCodeAlignment));
  Address last_gc_pc = NULL;
  for (RelocIterator it(this); !it.done(); it.next()) {
    it.rinfo()->Verify();
    // Ensure that GC will not iterate twice over the same pointer.
    if (RelocInfo::IsGCRelocMode(it.rinfo()->rmode())) {
      CHECK(it.rinfo()->pc() != last_gc_pc);
      last_gc_pc = it.rinfo()->pc();
    }
  }
}


void JSArray::JSArrayVerify() {
  JSObjectVerify();
  ASSERT(length()->IsNumber() || length()->IsUndefined());
  ASSERT(elements()->IsUndefined() || elements()->IsFixedArray());
}


void JSRegExp::JSRegExpVerify() {
  JSObjectVerify();
  ASSERT(data()->IsUndefined() || data()->IsFixedArray());
  switch (TypeTag()) {
    case JSRegExp::ATOM: {
      FixedArray* arr = FixedArray::cast(data());
      ASSERT(arr->get(JSRegExp::kAtomPatternIndex)->IsString());
      break;
    }
    case JSRegExp::IRREGEXP: {
      bool is_native = RegExpImpl::UsesNativeRegExp();

      FixedArray* arr = FixedArray::cast(data());
      Object* ascii_data = arr->get(JSRegExp::kIrregexpASCIICodeIndex);
      // TheHole : Not compiled yet.
      // JSObject: Compilation error.
      // Code/ByteArray: Compiled code.
      ASSERT(ascii_data->IsTheHole() || ascii_data->IsJSObject() ||
          (is_native ? ascii_data->IsCode() : ascii_data->IsByteArray()));
      Object* uc16_data = arr->get(JSRegExp::kIrregexpUC16CodeIndex);
      ASSERT(uc16_data->IsTheHole() || ascii_data->IsJSObject() ||
          (is_native ? uc16_data->IsCode() : uc16_data->IsByteArray()));
      ASSERT(arr->get(JSRegExp::kIrregexpCaptureCountIndex)->IsSmi());
      ASSERT(arr->get(JSRegExp::kIrregexpMaxRegisterCountIndex)->IsSmi());
      break;
    }
    default:
      ASSERT_EQ(JSRegExp::NOT_COMPILED, TypeTag());
      ASSERT(data()->IsUndefined());
      break;
  }
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void Proxy::ProxyPrint(FILE* out) {
  PrintF(out, "proxy to %p", proxy());
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void Proxy::ProxyVerify() {
  ASSERT(IsProxy());
}


void AccessorInfo::AccessorInfoVerify() {
  CHECK(IsAccessorInfo());
  VerifyPointer(getter());
  VerifyPointer(setter());
  VerifyPointer(name());
  VerifyPointer(data());
  VerifyPointer(flag());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void AccessorInfo::AccessorInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "AccessorInfo");
  PrintF(out, "\n - getter: ");
  getter()->ShortPrint(out);
  PrintF(out, "\n - setter: ");
  setter()->ShortPrint(out);
  PrintF(out, "\n - name: ");
  name()->ShortPrint(out);
  PrintF(out, "\n - data: ");
  data()->ShortPrint(out);
  PrintF(out, "\n - flag: ");
  flag()->ShortPrint(out);
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void AccessCheckInfo::AccessCheckInfoVerify() {
  CHECK(IsAccessCheckInfo());
  VerifyPointer(named_callback());
  VerifyPointer(indexed_callback());
  VerifyPointer(data());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void AccessCheckInfo::AccessCheckInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "AccessCheckInfo");
  PrintF(out, "\n - named_callback: ");
  named_callback()->ShortPrint(out);
  PrintF(out, "\n - indexed_callback: ");
  indexed_callback()->ShortPrint(out);
  PrintF(out, "\n - data: ");
  data()->ShortPrint(out);
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void InterceptorInfo::InterceptorInfoVerify() {
  CHECK(IsInterceptorInfo());
  VerifyPointer(getter());
  VerifyPointer(setter());
  VerifyPointer(query());
  VerifyPointer(deleter());
  VerifyPointer(enumerator());
  VerifyPointer(data());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void InterceptorInfo::InterceptorInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "InterceptorInfo");
  PrintF(out, "\n - getter: ");
  getter()->ShortPrint(out);
  PrintF(out, "\n - setter: ");
  setter()->ShortPrint(out);
  PrintF(out, "\n - query: ");
  query()->ShortPrint(out);
  PrintF(out, "\n - deleter: ");
  deleter()->ShortPrint(out);
  PrintF(out, "\n - enumerator: ");
  enumerator()->ShortPrint(out);
  PrintF(out, "\n - data: ");
  data()->ShortPrint(out);
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void CallHandlerInfo::CallHandlerInfoVerify() {
  CHECK(IsCallHandlerInfo());
  VerifyPointer(callback());
  VerifyPointer(data());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void CallHandlerInfo::CallHandlerInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "CallHandlerInfo");
  PrintF(out, "\n - callback: ");
  callback()->ShortPrint(out);
  PrintF(out, "\n - data: ");
  data()->ShortPrint(out);
  PrintF(out, "\n - call_stub_cache: ");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void TemplateInfo::TemplateInfoVerify() {
  VerifyPointer(tag());
  VerifyPointer(property_list());
}

void FunctionTemplateInfo::FunctionTemplateInfoVerify() {
  CHECK(IsFunctionTemplateInfo());
  TemplateInfoVerify();
  VerifyPointer(serial_number());
  VerifyPointer(call_code());
  VerifyPointer(property_accessors());
  VerifyPointer(prototype_template());
  VerifyPointer(parent_template());
  VerifyPointer(named_property_handler());
  VerifyPointer(indexed_property_handler());
  VerifyPointer(instance_template());
  VerifyPointer(signature());
  VerifyPointer(access_check_info());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void FunctionTemplateInfo::FunctionTemplateInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "FunctionTemplateInfo");
  PrintF(out, "\n - class name: ");
  class_name()->ShortPrint(out);
  PrintF(out, "\n - tag: ");
  tag()->ShortPrint(out);
  PrintF(out, "\n - property_list: ");
  property_list()->ShortPrint(out);
  PrintF(out, "\n - serial_number: ");
  serial_number()->ShortPrint(out);
  PrintF(out, "\n - call_code: ");
  call_code()->ShortPrint(out);
  PrintF(out, "\n - property_accessors: ");
  property_accessors()->ShortPrint(out);
  PrintF(out, "\n - prototype_template: ");
  prototype_template()->ShortPrint(out);
  PrintF(out, "\n - parent_template: ");
  parent_template()->ShortPrint(out);
  PrintF(out, "\n - named_property_handler: ");
  named_property_handler()->ShortPrint(out);
  PrintF(out, "\n - indexed_property_handler: ");
  indexed_property_handler()->ShortPrint(out);
  PrintF(out, "\n - instance_template: ");
  instance_template()->ShortPrint(out);
  PrintF(out, "\n - signature: ");
  signature()->ShortPrint(out);
  PrintF(out, "\n - access_check_info: ");
  access_check_info()->ShortPrint(out);
  PrintF(out, "\n - hidden_prototype: %s",
         hidden_prototype() ? "true" : "false");
  PrintF(out, "\n - undetectable: %s", undetectable() ? "true" : "false");
  PrintF(out, "\n - need_access_check: %s",
         needs_access_check() ? "true" : "false");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void ObjectTemplateInfo::ObjectTemplateInfoVerify() {
  CHECK(IsObjectTemplateInfo());
  TemplateInfoVerify();
  VerifyPointer(constructor());
  VerifyPointer(internal_field_count());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void ObjectTemplateInfo::ObjectTemplateInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "ObjectTemplateInfo");
  PrintF(out, "\n - constructor: ");
  constructor()->ShortPrint(out);
  PrintF(out, "\n - internal_field_count: ");
  internal_field_count()->ShortPrint(out);
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void SignatureInfo::SignatureInfoVerify() {
  CHECK(IsSignatureInfo());
  VerifyPointer(receiver());
  VerifyPointer(args());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void SignatureInfo::SignatureInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "SignatureInfo");
  PrintF(out, "\n - receiver: ");
  receiver()->ShortPrint(out);
  PrintF(out, "\n - args: ");
  args()->ShortPrint(out);
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void TypeSwitchInfo::TypeSwitchInfoVerify() {
  CHECK(IsTypeSwitchInfo());
  VerifyPointer(types());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void TypeSwitchInfo::TypeSwitchInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "TypeSwitchInfo");
  PrintF(out, "\n - types: ");
  types()->ShortPrint(out);
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void Script::ScriptVerify() {
  CHECK(IsScript());
  VerifyPointer(source());
  VerifyPointer(name());
  line_offset()->SmiVerify();
  column_offset()->SmiVerify();
  VerifyPointer(data());
  VerifyPointer(wrapper());
  type()->SmiVerify();
  VerifyPointer(line_ends());
  VerifyPointer(id());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void Script::ScriptPrint(FILE* out) {
  HeapObject::PrintHeader(out, "Script");
  PrintF(out, "\n - source: ");
  source()->ShortPrint(out);
  PrintF(out, "\n - name: ");
  name()->ShortPrint(out);
  PrintF(out, "\n - line_offset: ");
  line_offset()->ShortPrint(out);
  PrintF(out, "\n - column_offset: ");
  column_offset()->ShortPrint(out);
  PrintF(out, "\n - type: ");
  type()->ShortPrint(out);
  PrintF(out, "\n - id: ");
  id()->ShortPrint(out);
  PrintF(out, "\n - data: ");
  data()->ShortPrint(out);
  PrintF(out, "\n - context data: ");
  context_data()->ShortPrint(out);
  PrintF(out, "\n - wrapper: ");
  wrapper()->ShortPrint(out);
  PrintF(out, "\n - compilation type: ");
  compilation_type()->ShortPrint(out);
  PrintF(out, "\n - line ends: ");
  line_ends()->ShortPrint(out);
  PrintF(out, "\n - eval from shared: ");
  eval_from_shared()->ShortPrint(out);
  PrintF(out, "\n - eval from instructions offset: ");
  eval_from_instructions_offset()->ShortPrint(out);
  PrintF(out, "\n");
}
#endif  // OBJECT_PRINT


#ifdef ENABLE_DEBUGGER_SUPPORT
#ifdef DEBUG
void DebugInfo::DebugInfoVerify() {
  CHECK(IsDebugInfo());
  VerifyPointer(shared());
  VerifyPointer(original_code());
  VerifyPointer(code());
  VerifyPointer(break_points());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void DebugInfo::DebugInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "DebugInfo");
  PrintF(out, "\n - shared: ");
  shared()->ShortPrint(out);
  PrintF(out, "\n - original_code: ");
  original_code()->ShortPrint(out);
  PrintF(out, "\n - code: ");
  code()->ShortPrint(out);
  PrintF(out, "\n - break_points: ");
  break_points()->Print(out);
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
void BreakPointInfo::BreakPointInfoVerify() {
  CHECK(IsBreakPointInfo());
  code_position()->SmiVerify();
  source_position()->SmiVerify();
  statement_position()->SmiVerify();
  VerifyPointer(break_point_objects());
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void BreakPointInfo::BreakPointInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "BreakPointInfo");
  PrintF(out, "\n - code_position: %d", code_position()->value());
  PrintF(out, "\n - source_position: %d", source_position()->value());
  PrintF(out, "\n - statement_position: %d", statement_position()->value());
  PrintF(out, "\n - break_point_objects: ");
  break_point_objects()->ShortPrint(out);
}
#endif  // OBJECT_PRINT
#endif  // ENABLE_DEBUGGER_SUPPORT


#ifdef DEBUG
void JSObject::IncrementSpillStatistics(SpillInformation* info) {
  info->number_of_objects_++;
  // Named properties
  if (HasFastProperties()) {
    info->number_of_objects_with_fast_properties_++;
    info->number_of_fast_used_fields_   += map()->NextFreePropertyIndex();
    info->number_of_fast_unused_fields_ += map()->unused_property_fields();
  } else {
    StringDictionary* dict = property_dictionary();
    info->number_of_slow_used_properties_ += dict->NumberOfElements();
    info->number_of_slow_unused_properties_ +=
        dict->Capacity() - dict->NumberOfElements();
  }
  // Indexed properties
  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      info->number_of_objects_with_fast_elements_++;
      int holes = 0;
      FixedArray* e = FixedArray::cast(elements());
      int len = e->length();
      for (int i = 0; i < len; i++) {
        if (e->get(i) == Heap::the_hole_value()) holes++;
      }
      info->number_of_fast_used_elements_   += len - holes;
      info->number_of_fast_unused_elements_ += holes;
      break;
    }
    case PIXEL_ELEMENTS: {
      info->number_of_objects_with_fast_elements_++;
      PixelArray* e = PixelArray::cast(elements());
      info->number_of_fast_used_elements_ += e->length();
      break;
    }
    case DICTIONARY_ELEMENTS: {
      NumberDictionary* dict = element_dictionary();
      info->number_of_slow_used_elements_ += dict->NumberOfElements();
      info->number_of_slow_unused_elements_ +=
          dict->Capacity() - dict->NumberOfElements();
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
}


void JSObject::SpillInformation::Clear() {
  number_of_objects_ = 0;
  number_of_objects_with_fast_properties_ = 0;
  number_of_objects_with_fast_elements_ = 0;
  number_of_fast_used_fields_ = 0;
  number_of_fast_unused_fields_ = 0;
  number_of_slow_used_properties_ = 0;
  number_of_slow_unused_properties_ = 0;
  number_of_fast_used_elements_ = 0;
  number_of_fast_unused_elements_ = 0;
  number_of_slow_used_elements_ = 0;
  number_of_slow_unused_elements_ = 0;
}

void JSObject::SpillInformation::Print() {
  PrintF("\n  JSObject Spill Statistics (#%d):\n", number_of_objects_);

  PrintF("    - fast properties (#%d): %d (used) %d (unused)\n",
         number_of_objects_with_fast_properties_,
         number_of_fast_used_fields_, number_of_fast_unused_fields_);

  PrintF("    - slow properties (#%d): %d (used) %d (unused)\n",
         number_of_objects_ - number_of_objects_with_fast_properties_,
         number_of_slow_used_properties_, number_of_slow_unused_properties_);

  PrintF("    - fast elements (#%d): %d (used) %d (unused)\n",
         number_of_objects_with_fast_elements_,
         number_of_fast_used_elements_, number_of_fast_unused_elements_);

  PrintF("    - slow elements (#%d): %d (used) %d (unused)\n",
         number_of_objects_ - number_of_objects_with_fast_elements_,
         number_of_slow_used_elements_, number_of_slow_unused_elements_);

  PrintF("\n");
}
#endif  // DEBUG


#ifdef OBJECT_PRINT
void DescriptorArray::PrintDescriptors(FILE* out) {
  PrintF(out, "Descriptor array  %d\n", number_of_descriptors());
  for (int i = 0; i < number_of_descriptors(); i++) {
    PrintF(out, " %d: ", i);
    Descriptor desc;
    Get(i, &desc);
    desc.Print(out);
  }
  PrintF(out, "\n");
}
#endif  // OBJECT_PRINT


#ifdef DEBUG
bool DescriptorArray::IsSortedNoDuplicates() {
  String* current_key = NULL;
  uint32_t current = 0;
  for (int i = 0; i < number_of_descriptors(); i++) {
    String* key = GetKey(i);
    if (key == current_key) {
      PrintDescriptors();
      return false;
    }
    current_key = key;
    uint32_t hash = GetKey(i)->Hash();
    if (hash < current) {
      PrintDescriptors();
      return false;
    }
    current = hash;
  }
  return true;
}


void JSFunctionResultCache::JSFunctionResultCacheVerify() {
  JSFunction::cast(get(kFactoryIndex))->Verify();

  int size = Smi::cast(get(kCacheSizeIndex))->value();
  ASSERT(kEntriesIndex <= size);
  ASSERT(size <= length());
  ASSERT_EQ(0, size % kEntrySize);

  int finger = Smi::cast(get(kFingerIndex))->value();
  ASSERT(kEntriesIndex <= finger);
  ASSERT(finger < size || finger == kEntriesIndex);
  ASSERT_EQ(0, finger % kEntrySize);

  if (FLAG_enable_slow_asserts) {
    for (int i = kEntriesIndex; i < size; i++) {
      ASSERT(!get(i)->IsTheHole());
      get(i)->Verify();
    }
    for (int i = size; i < length(); i++) {
      ASSERT(get(i)->IsTheHole());
      get(i)->Verify();
    }
  }
}


void NormalizedMapCache::NormalizedMapCacheVerify() {
  FixedArray::cast(this)->Verify();
  if (FLAG_enable_slow_asserts) {
    for (int i = 0; i < length(); i++) {
      Object* e = get(i);
      if (e->IsMap()) {
        Map::cast(e)->SharedMapVerify();
      } else {
        ASSERT(e->IsUndefined());
      }
    }
  }
}


#endif  // DEBUG

} }  // namespace v8::internal

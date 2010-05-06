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

namespace v8 {
namespace internal {

#ifdef DEBUG

static const char* TypeToString(InstanceType type);


void Object::Print() {
  if (IsSmi()) {
    Smi::cast(this)->SmiPrint();
  } else if (IsFailure()) {
    Failure::cast(this)->FailurePrint();
  } else {
    HeapObject::cast(this)->HeapObjectPrint();
  }
  Flush();
}


void Object::PrintLn() {
  Print();
  PrintF("\n");
}


void Object::Verify() {
  if (IsSmi()) {
    Smi::cast(this)->SmiVerify();
  } else if (IsFailure()) {
    Failure::cast(this)->FailureVerify();
  } else {
    HeapObject::cast(this)->HeapObjectVerify();
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


void HeapObject::PrintHeader(const char* id) {
  PrintF("%p: [%s]\n", this, id);
}


void HeapObject::HeapObjectPrint() {
  InstanceType instance_type = map()->instance_type();

  HandleScope scope;
  if (instance_type < FIRST_NONSTRING_TYPE) {
    String::cast(this)->StringPrint();
    return;
  }

  switch (instance_type) {
    case MAP_TYPE:
      Map::cast(this)->MapPrint();
      break;
    case HEAP_NUMBER_TYPE:
      HeapNumber::cast(this)->HeapNumberPrint();
      break;
    case FIXED_ARRAY_TYPE:
      FixedArray::cast(this)->FixedArrayPrint();
      break;
    case BYTE_ARRAY_TYPE:
      ByteArray::cast(this)->ByteArrayPrint();
      break;
    case PIXEL_ARRAY_TYPE:
      PixelArray::cast(this)->PixelArrayPrint();
      break;
    case EXTERNAL_BYTE_ARRAY_TYPE:
      ExternalByteArray::cast(this)->ExternalByteArrayPrint();
      break;
    case EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
      ExternalUnsignedByteArray::cast(this)->ExternalUnsignedByteArrayPrint();
      break;
    case EXTERNAL_SHORT_ARRAY_TYPE:
      ExternalShortArray::cast(this)->ExternalShortArrayPrint();
      break;
    case EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
      ExternalUnsignedShortArray::cast(this)->ExternalUnsignedShortArrayPrint();
      break;
    case EXTERNAL_INT_ARRAY_TYPE:
      ExternalIntArray::cast(this)->ExternalIntArrayPrint();
      break;
    case EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
      ExternalUnsignedIntArray::cast(this)->ExternalUnsignedIntArrayPrint();
      break;
    case EXTERNAL_FLOAT_ARRAY_TYPE:
      ExternalFloatArray::cast(this)->ExternalFloatArrayPrint();
      break;
    case FILLER_TYPE:
      PrintF("filler");
      break;
    case JS_OBJECT_TYPE:  // fall through
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_ARRAY_TYPE:
    case JS_REGEXP_TYPE:
      JSObject::cast(this)->JSObjectPrint();
      break;
    case ODDBALL_TYPE:
      Oddball::cast(this)->to_string()->Print();
      break;
    case JS_FUNCTION_TYPE:
      JSFunction::cast(this)->JSFunctionPrint();
      break;
    case JS_GLOBAL_PROXY_TYPE:
      JSGlobalProxy::cast(this)->JSGlobalProxyPrint();
      break;
    case JS_GLOBAL_OBJECT_TYPE:
      JSGlobalObject::cast(this)->JSGlobalObjectPrint();
      break;
    case JS_BUILTINS_OBJECT_TYPE:
      JSBuiltinsObject::cast(this)->JSBuiltinsObjectPrint();
      break;
    case JS_VALUE_TYPE:
      PrintF("Value wrapper around:");
      JSValue::cast(this)->value()->Print();
      break;
    case CODE_TYPE:
      Code::cast(this)->CodePrint();
      break;
    case PROXY_TYPE:
      Proxy::cast(this)->ProxyPrint();
      break;
    case SHARED_FUNCTION_INFO_TYPE:
      SharedFunctionInfo::cast(this)->SharedFunctionInfoPrint();
      break;
    case JS_GLOBAL_PROPERTY_CELL_TYPE:
      JSGlobalPropertyCell::cast(this)->JSGlobalPropertyCellPrint();
      break;
#define MAKE_STRUCT_CASE(NAME, Name, name) \
  case NAME##_TYPE:                        \
    Name::cast(this)->Name##Print();       \
    break;
  STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE

    default:
      PrintF("UNKNOWN TYPE %d", map()->instance_type());
      UNREACHABLE();
      break;
  }
}


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


void ByteArray::ByteArrayPrint() {
  PrintF("byte array, data starts at %p", GetDataStartAddress());
}


void PixelArray::PixelArrayPrint() {
  PrintF("pixel array");
}


void ExternalByteArray::ExternalByteArrayPrint() {
  PrintF("external byte array");
}


void ExternalUnsignedByteArray::ExternalUnsignedByteArrayPrint() {
  PrintF("external unsigned byte array");
}


void ExternalShortArray::ExternalShortArrayPrint() {
  PrintF("external short array");
}


void ExternalUnsignedShortArray::ExternalUnsignedShortArrayPrint() {
  PrintF("external unsigned short array");
}


void ExternalIntArray::ExternalIntArrayPrint() {
  PrintF("external int array");
}


void ExternalUnsignedIntArray::ExternalUnsignedIntArrayPrint() {
  PrintF("external unsigned int array");
}


void ExternalFloatArray::ExternalFloatArrayPrint() {
  PrintF("external float array");
}


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


void JSObject::PrintProperties() {
  if (HasFastProperties()) {
    DescriptorArray* descs = map()->instance_descriptors();
    for (int i = 0; i < descs->number_of_descriptors(); i++) {
      PrintF("   ");
      descs->GetKey(i)->StringPrint();
      PrintF(": ");
      switch (descs->GetType(i)) {
        case FIELD: {
          int index = descs->GetFieldIndex(i);
          FastPropertyAt(index)->ShortPrint();
          PrintF(" (field at offset %d)\n", index);
          break;
        }
        case CONSTANT_FUNCTION:
          descs->GetConstantFunction(i)->ShortPrint();
          PrintF(" (constant function)\n");
          break;
        case CALLBACKS:
          descs->GetCallbacksObject(i)->ShortPrint();
          PrintF(" (callback)\n");
          break;
        case MAP_TRANSITION:
          PrintF(" (map transition)\n");
          break;
        case CONSTANT_TRANSITION:
          PrintF(" (constant transition)\n");
          break;
        case NULL_DESCRIPTOR:
          PrintF(" (null descriptor)\n");
          break;
        default:
          UNREACHABLE();
          break;
      }
    }
  } else {
    property_dictionary()->Print();
  }
}


void JSObject::PrintElements() {
  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      // Print in array notation for non-sparse arrays.
      FixedArray* p = FixedArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF("   %d: ", i);
        p->get(i)->ShortPrint();
        PrintF("\n");
      }
      break;
    }
    case PIXEL_ELEMENTS: {
      PixelArray* p = PixelArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF("   %d: %d\n", i, p->get(i));
      }
      break;
    }
    case EXTERNAL_BYTE_ELEMENTS: {
      ExternalByteArray* p = ExternalByteArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF("   %d: %d\n", i, static_cast<int>(p->get(i)));
      }
      break;
    }
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS: {
      ExternalUnsignedByteArray* p =
          ExternalUnsignedByteArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF("   %d: %d\n", i, static_cast<int>(p->get(i)));
      }
      break;
    }
    case EXTERNAL_SHORT_ELEMENTS: {
      ExternalShortArray* p = ExternalShortArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF("   %d: %d\n", i, static_cast<int>(p->get(i)));
      }
      break;
    }
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS: {
      ExternalUnsignedShortArray* p =
          ExternalUnsignedShortArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF("   %d: %d\n", i, static_cast<int>(p->get(i)));
      }
      break;
    }
    case EXTERNAL_INT_ELEMENTS: {
      ExternalIntArray* p = ExternalIntArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF("   %d: %d\n", i, static_cast<int>(p->get(i)));
      }
      break;
    }
    case EXTERNAL_UNSIGNED_INT_ELEMENTS: {
      ExternalUnsignedIntArray* p =
          ExternalUnsignedIntArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF("   %d: %d\n", i, static_cast<int>(p->get(i)));
      }
      break;
    }
    case EXTERNAL_FLOAT_ELEMENTS: {
      ExternalFloatArray* p = ExternalFloatArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF("   %d: %f\n", i, p->get(i));
      }
      break;
    }
    case DICTIONARY_ELEMENTS:
      elements()->Print();
      break;
    default:
      UNREACHABLE();
      break;
  }
}


void JSObject::JSObjectPrint() {
  PrintF("%p: [JSObject]\n", this);
  PrintF(" - map = %p\n", map());
  PrintF(" - prototype = %p\n", GetPrototype());
  PrintF(" {\n");
  PrintProperties();
  PrintElements();
  PrintF(" }\n");
}


void JSObject::JSObjectVerify() {
  VerifyHeapPointer(properties());
  VerifyHeapPointer(elements());
  if (HasFastProperties()) {
    CHECK_EQ(map()->unused_property_fields(),
             (map()->inobject_properties() + properties()->length() -
              map()->NextFreePropertyIndex()));
  }
}


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
    case EXTERNAL_SYMBOL_TYPE: return "EXTERNAL_SYMBOL";
    case ASCII_STRING_TYPE: return "ASCII_STRING";
    case STRING_TYPE: return "TWO_BYTE_STRING";
    case CONS_STRING_TYPE:
    case CONS_ASCII_STRING_TYPE: return "CONS_STRING";
    case EXTERNAL_ASCII_STRING_TYPE:
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


void Map::MapPrint() {
  HeapObject::PrintHeader("Map");
  PrintF(" - type: %s\n", TypeToString(instance_type()));
  PrintF(" - instance size: %d\n", instance_size());
  PrintF(" - inobject properties: %d\n", inobject_properties());
  PrintF(" - pre-allocated property fields: %d\n",
      pre_allocated_property_fields());
  PrintF(" - unused property fields: %d\n", unused_property_fields());
  if (is_hidden_prototype()) {
    PrintF(" - hidden_prototype\n");
  }
  if (has_named_interceptor()) {
    PrintF(" - named_interceptor\n");
  }
  if (has_indexed_interceptor()) {
    PrintF(" - indexed_interceptor\n");
  }
  if (is_undetectable()) {
    PrintF(" - undetectable\n");
  }
  if (has_instance_call_handler()) {
    PrintF(" - instance_call_handler\n");
  }
  if (is_access_check_needed()) {
    PrintF(" - access_check_needed\n");
  }
  PrintF(" - instance descriptors: ");
  instance_descriptors()->ShortPrint();
  PrintF("\n - prototype: ");
  prototype()->ShortPrint();
  PrintF("\n - constructor: ");
  constructor()->ShortPrint();
  PrintF("\n");
}


void Map::MapVerify() {
  ASSERT(!Heap::InNewSpace(this));
  ASSERT(FIRST_TYPE <= instance_type() && instance_type() <= LAST_TYPE);
  ASSERT(kPointerSize <= instance_size()
         && instance_size() < Heap::Capacity());
  VerifyHeapPointer(prototype());
  VerifyHeapPointer(instance_descriptors());
}


void CodeCache::CodeCachePrint() {
  HeapObject::PrintHeader("CodeCache");
  PrintF("\n - default_cache: ");
  default_cache()->ShortPrint();
  PrintF("\n - normal_type_cache: ");
  normal_type_cache()->ShortPrint();
}


void CodeCache::CodeCacheVerify() {
  VerifyHeapPointer(default_cache());
  VerifyHeapPointer(normal_type_cache());
  ASSERT(default_cache()->IsFixedArray());
  ASSERT(normal_type_cache()->IsUndefined()
         || normal_type_cache()->IsCodeCacheHashTable());
}


void FixedArray::FixedArrayPrint() {
  HeapObject::PrintHeader("FixedArray");
  PrintF(" - length: %d", length());
  for (int i = 0; i < length(); i++) {
    PrintF("\n  [%d]: ", i);
    get(i)->ShortPrint();
  }
  PrintF("\n");
}


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


void JSValue::JSValuePrint() {
  HeapObject::PrintHeader("ValueObject");
  value()->Print();
}


void JSValue::JSValueVerify() {
  Object* v = value();
  if (v->IsHeapObject()) {
    VerifyHeapPointer(v);
  }
}


void String::StringPrint() {
  if (StringShape(this).IsSymbol()) {
    PrintF("#");
  } else if (StringShape(this).IsCons()) {
    PrintF("c\"");
  } else {
    PrintF("\"");
  }

  for (int i = 0; i < length(); i++) {
    PrintF("%c", Get(i));
  }

  if (!StringShape(this).IsSymbol()) PrintF("\"");
}


void String::StringVerify() {
  CHECK(IsString());
  CHECK(length() >= 0 && length() <= Smi::kMaxValue);
  if (IsSymbol()) {
    CHECK(!Heap::InNewSpace(this));
  }
}


void JSFunction::JSFunctionPrint() {
  HeapObject::PrintHeader("Function");
  PrintF(" - map = 0x%p\n", map());
  PrintF(" - initial_map = ");
  if (has_initial_map()) {
    initial_map()->ShortPrint();
  }
  PrintF("\n - shared_info = ");
  shared()->ShortPrint();
  PrintF("\n   - name = ");
  shared()->name()->Print();
  PrintF("\n - context = ");
  unchecked_context()->ShortPrint();
  PrintF("\n - code = ");
  code()->ShortPrint();
  PrintF("\n");

  PrintProperties();
  PrintElements();

  PrintF("\n");
}


void JSFunction::JSFunctionVerify() {
  CHECK(IsJSFunction());
  VerifyObjectField(kPrototypeOrInitialMapOffset);
}


void SharedFunctionInfo::SharedFunctionInfoPrint() {
  HeapObject::PrintHeader("SharedFunctionInfo");
  PrintF(" - name: ");
  name()->ShortPrint();
  PrintF("\n - expected_nof_properties: %d", expected_nof_properties());
  PrintF("\n - instance class name = ");
  instance_class_name()->Print();
  PrintF("\n - code = ");
  code()->ShortPrint();
  PrintF("\n - source code = ");
  GetSourceCode()->ShortPrint();
  // Script files are often large, hard to read.
  // PrintF("\n - script =");
  // script()->Print();
  PrintF("\n - function token position = %d", function_token_position());
  PrintF("\n - start position = %d", start_position());
  PrintF("\n - end position = %d", end_position());
  PrintF("\n - is expression = %d", is_expression());
  PrintF("\n - debug info = ");
  debug_info()->ShortPrint();
  PrintF("\n - length = %d", length());
  PrintF("\n - has_only_simple_this_property_assignments = %d",
         has_only_simple_this_property_assignments());
  PrintF("\n - this_property_assignments = ");
  this_property_assignments()->ShortPrint();
  PrintF("\n");
}

void SharedFunctionInfo::SharedFunctionInfoVerify() {
  CHECK(IsSharedFunctionInfo());
  VerifyObjectField(kNameOffset);
  VerifyObjectField(kCodeOffset);
  VerifyObjectField(kInstanceClassNameOffset);
  VerifyObjectField(kFunctionDataOffset);
  VerifyObjectField(kScriptOffset);
  VerifyObjectField(kDebugInfoOffset);
}


void JSGlobalProxy::JSGlobalProxyPrint() {
  PrintF("global_proxy");
  JSObjectPrint();
  PrintF("context : ");
  context()->ShortPrint();
  PrintF("\n");
}


void JSGlobalProxy::JSGlobalProxyVerify() {
  CHECK(IsJSGlobalProxy());
  JSObjectVerify();
  VerifyObjectField(JSGlobalProxy::kContextOffset);
  // Make sure that this object has no properties, elements.
  CHECK_EQ(0, properties()->length());
  CHECK_EQ(0, elements()->length());
}


void JSGlobalObject::JSGlobalObjectPrint() {
  PrintF("global ");
  JSObjectPrint();
  PrintF("global context : ");
  global_context()->ShortPrint();
  PrintF("\n");
}


void JSGlobalObject::JSGlobalObjectVerify() {
  CHECK(IsJSGlobalObject());
  JSObjectVerify();
  for (int i = GlobalObject::kBuiltinsOffset;
       i < JSGlobalObject::kSize;
       i += kPointerSize) {
    VerifyObjectField(i);
  }
}


void JSBuiltinsObject::JSBuiltinsObjectPrint() {
  PrintF("builtins ");
  JSObjectPrint();
}


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


void JSGlobalPropertyCell::JSGlobalPropertyCellPrint() {
  HeapObject::PrintHeader("JSGlobalPropertyCell");
}


void Code::CodePrint() {
  HeapObject::PrintHeader("Code");
#ifdef ENABLE_DISASSEMBLER
  Disassemble(NULL);
#endif
}


void Code::CodeVerify() {
  CHECK(IsAligned(reinterpret_cast<intptr_t>(instruction_start()),
                  static_cast<intptr_t>(kCodeAlignment)));
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


void Proxy::ProxyPrint() {
  PrintF("proxy to %p", proxy());
}


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
  VerifyPointer(load_stub_cache());
}

void AccessorInfo::AccessorInfoPrint() {
  HeapObject::PrintHeader("AccessorInfo");
  PrintF("\n - getter: ");
  getter()->ShortPrint();
  PrintF("\n - setter: ");
  setter()->ShortPrint();
  PrintF("\n - name: ");
  name()->ShortPrint();
  PrintF("\n - data: ");
  data()->ShortPrint();
  PrintF("\n - flag: ");
  flag()->ShortPrint();
}

void AccessCheckInfo::AccessCheckInfoVerify() {
  CHECK(IsAccessCheckInfo());
  VerifyPointer(named_callback());
  VerifyPointer(indexed_callback());
  VerifyPointer(data());
}

void AccessCheckInfo::AccessCheckInfoPrint() {
  HeapObject::PrintHeader("AccessCheckInfo");
  PrintF("\n - named_callback: ");
  named_callback()->ShortPrint();
  PrintF("\n - indexed_callback: ");
  indexed_callback()->ShortPrint();
  PrintF("\n - data: ");
  data()->ShortPrint();
}

void InterceptorInfo::InterceptorInfoVerify() {
  CHECK(IsInterceptorInfo());
  VerifyPointer(getter());
  VerifyPointer(setter());
  VerifyPointer(query());
  VerifyPointer(deleter());
  VerifyPointer(enumerator());
  VerifyPointer(data());
}

void InterceptorInfo::InterceptorInfoPrint() {
  HeapObject::PrintHeader("InterceptorInfo");
  PrintF("\n - getter: ");
  getter()->ShortPrint();
  PrintF("\n - setter: ");
  setter()->ShortPrint();
  PrintF("\n - query: ");
  query()->ShortPrint();
  PrintF("\n - deleter: ");
  deleter()->ShortPrint();
  PrintF("\n - enumerator: ");
  enumerator()->ShortPrint();
  PrintF("\n - data: ");
  data()->ShortPrint();
}

void CallHandlerInfo::CallHandlerInfoVerify() {
  CHECK(IsCallHandlerInfo());
  VerifyPointer(callback());
  VerifyPointer(data());
}

void CallHandlerInfo::CallHandlerInfoPrint() {
  HeapObject::PrintHeader("CallHandlerInfo");
  PrintF("\n - callback: ");
  callback()->ShortPrint();
  PrintF("\n - data: ");
  data()->ShortPrint();
}

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

void FunctionTemplateInfo::FunctionTemplateInfoPrint() {
  HeapObject::PrintHeader("FunctionTemplateInfo");
  PrintF("\n - class name: ");
  class_name()->ShortPrint();
  PrintF("\n - tag: ");
  tag()->ShortPrint();
  PrintF("\n - property_list: ");
  property_list()->ShortPrint();
  PrintF("\n - serial_number: ");
  serial_number()->ShortPrint();
  PrintF("\n - call_code: ");
  call_code()->ShortPrint();
  PrintF("\n - property_accessors: ");
  property_accessors()->ShortPrint();
  PrintF("\n - prototype_template: ");
  prototype_template()->ShortPrint();
  PrintF("\n - parent_template: ");
  parent_template()->ShortPrint();
  PrintF("\n - named_property_handler: ");
  named_property_handler()->ShortPrint();
  PrintF("\n - indexed_property_handler: ");
  indexed_property_handler()->ShortPrint();
  PrintF("\n - instance_template: ");
  instance_template()->ShortPrint();
  PrintF("\n - signature: ");
  signature()->ShortPrint();
  PrintF("\n - access_check_info: ");
  access_check_info()->ShortPrint();
  PrintF("\n - hidden_prototype: %s", hidden_prototype() ? "true" : "false");
  PrintF("\n - undetectable: %s", undetectable() ? "true" : "false");
  PrintF("\n - need_access_check: %s", needs_access_check() ? "true" : "false");
}

void ObjectTemplateInfo::ObjectTemplateInfoVerify() {
  CHECK(IsObjectTemplateInfo());
  TemplateInfoVerify();
  VerifyPointer(constructor());
  VerifyPointer(internal_field_count());
}

void ObjectTemplateInfo::ObjectTemplateInfoPrint() {
  HeapObject::PrintHeader("ObjectTemplateInfo");
  PrintF("\n - constructor: ");
  constructor()->ShortPrint();
  PrintF("\n - internal_field_count: ");
  internal_field_count()->ShortPrint();
}

void SignatureInfo::SignatureInfoVerify() {
  CHECK(IsSignatureInfo());
  VerifyPointer(receiver());
  VerifyPointer(args());
}

void SignatureInfo::SignatureInfoPrint() {
  HeapObject::PrintHeader("SignatureInfo");
  PrintF("\n - receiver: ");
  receiver()->ShortPrint();
  PrintF("\n - args: ");
  args()->ShortPrint();
}

void TypeSwitchInfo::TypeSwitchInfoVerify() {
  CHECK(IsTypeSwitchInfo());
  VerifyPointer(types());
}

void TypeSwitchInfo::TypeSwitchInfoPrint() {
  HeapObject::PrintHeader("TypeSwitchInfo");
  PrintF("\n - types: ");
  types()->ShortPrint();
}


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


void Script::ScriptPrint() {
  HeapObject::PrintHeader("Script");
  PrintF("\n - source: ");
  source()->ShortPrint();
  PrintF("\n - name: ");
  name()->ShortPrint();
  PrintF("\n - line_offset: ");
  line_offset()->ShortPrint();
  PrintF("\n - column_offset: ");
  column_offset()->ShortPrint();
  PrintF("\n - type: ");
  type()->ShortPrint();
  PrintF("\n - id: ");
  id()->ShortPrint();
  PrintF("\n - data: ");
  data()->ShortPrint();
  PrintF("\n - context data: ");
  context_data()->ShortPrint();
  PrintF("\n - wrapper: ");
  wrapper()->ShortPrint();
  PrintF("\n - compilation type: ");
  compilation_type()->ShortPrint();
  PrintF("\n - line ends: ");
  line_ends()->ShortPrint();
  PrintF("\n - eval from shared: ");
  eval_from_shared()->ShortPrint();
  PrintF("\n - eval from instructions offset: ");
  eval_from_instructions_offset()->ShortPrint();
  PrintF("\n");
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void DebugInfo::DebugInfoVerify() {
  CHECK(IsDebugInfo());
  VerifyPointer(shared());
  VerifyPointer(original_code());
  VerifyPointer(code());
  VerifyPointer(break_points());
}


void DebugInfo::DebugInfoPrint() {
  HeapObject::PrintHeader("DebugInfo");
  PrintF("\n - shared: ");
  shared()->ShortPrint();
  PrintF("\n - original_code: ");
  original_code()->ShortPrint();
  PrintF("\n - code: ");
  code()->ShortPrint();
  PrintF("\n - break_points: ");
  break_points()->Print();
}


void BreakPointInfo::BreakPointInfoVerify() {
  CHECK(IsBreakPointInfo());
  code_position()->SmiVerify();
  source_position()->SmiVerify();
  statement_position()->SmiVerify();
  VerifyPointer(break_point_objects());
}


void BreakPointInfo::BreakPointInfoPrint() {
  HeapObject::PrintHeader("BreakPointInfo");
  PrintF("\n - code_position: %d", code_position());
  PrintF("\n - source_position: %d", source_position());
  PrintF("\n - statement_position: %d", statement_position());
  PrintF("\n - break_point_objects: ");
  break_point_objects()->ShortPrint();
}
#endif


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


void DescriptorArray::PrintDescriptors() {
  PrintF("Descriptor array  %d\n", number_of_descriptors());
  for (int i = 0; i < number_of_descriptors(); i++) {
    PrintF(" %d: ", i);
    Descriptor desc;
    Get(i, &desc);
    desc.Print();
  }
  PrintF("\n");
}


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


#endif  // DEBUG

} }  // namespace v8::internal

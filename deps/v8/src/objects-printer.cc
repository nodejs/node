// Copyright 2011 the V8 project authors. All rights reserved.
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
    case EXTERNAL_PIXEL_ARRAY_TYPE:
      ExternalPixelArray::cast(this)->ExternalPixelArrayPrint(out);
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
    case EXTERNAL_DOUBLE_ARRAY_TYPE:
      ExternalDoubleArray::cast(this)->ExternalDoubleArrayPrint(out);
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
    case JS_PROXY_TYPE:
      JSProxy::cast(this)->JSProxyPrint(out);
      break;
    case JS_FUNCTION_PROXY_TYPE:
      JSFunctionProxy::cast(this)->JSFunctionProxyPrint(out);
      break;
    case JS_WEAK_MAP_TYPE:
      JSWeakMap::cast(this)->JSWeakMapPrint(out);
      break;
    case FOREIGN_TYPE:
      Foreign::cast(this)->ForeignPrint(out);
      break;
    case SHARED_FUNCTION_INFO_TYPE:
      SharedFunctionInfo::cast(this)->SharedFunctionInfoPrint(out);
      break;
    case JS_MESSAGE_OBJECT_TYPE:
      JSMessageObject::cast(this)->JSMessageObjectPrint(out);
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


void ByteArray::ByteArrayPrint(FILE* out) {
  PrintF(out, "byte array, data starts at %p", GetDataStartAddress());
}


void ExternalPixelArray::ExternalPixelArrayPrint(FILE* out) {
  PrintF(out, "external pixel array");
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


void ExternalDoubleArray::ExternalDoubleArrayPrint(FILE* out) {
  PrintF(out, "external double array");
}


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
    case FAST_DOUBLE_ELEMENTS: {
      // Print in array notation for non-sparse arrays.
      FixedDoubleArray* p = FixedDoubleArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        if (p->is_the_hole(i)) {
          PrintF(out, "   %d: <the hole>", i);
        } else {
          PrintF(out, "   %d: %g", i, p->get_scalar(i));
        }
        PrintF(out, "\n");
      }
      break;
    }
    case EXTERNAL_PIXEL_ELEMENTS: {
      ExternalPixelArray* p = ExternalPixelArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, p->get_scalar(i));
      }
      break;
    }
    case EXTERNAL_BYTE_ELEMENTS: {
      ExternalByteArray* p = ExternalByteArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, static_cast<int>(p->get_scalar(i)));
      }
      break;
    }
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS: {
      ExternalUnsignedByteArray* p =
          ExternalUnsignedByteArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, static_cast<int>(p->get_scalar(i)));
      }
      break;
    }
    case EXTERNAL_SHORT_ELEMENTS: {
      ExternalShortArray* p = ExternalShortArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, static_cast<int>(p->get_scalar(i)));
      }
      break;
    }
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS: {
      ExternalUnsignedShortArray* p =
          ExternalUnsignedShortArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, static_cast<int>(p->get_scalar(i)));
      }
      break;
    }
    case EXTERNAL_INT_ELEMENTS: {
      ExternalIntArray* p = ExternalIntArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, static_cast<int>(p->get_scalar(i)));
      }
      break;
    }
    case EXTERNAL_UNSIGNED_INT_ELEMENTS: {
      ExternalUnsignedIntArray* p =
          ExternalUnsignedIntArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %d\n", i, static_cast<int>(p->get_scalar(i)));
      }
      break;
    }
    case EXTERNAL_FLOAT_ELEMENTS: {
      ExternalFloatArray* p = ExternalFloatArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "   %d: %f\n", i, p->get_scalar(i));
      }
      break;
    }
    case EXTERNAL_DOUBLE_ELEMENTS: {
      ExternalDoubleArray* p = ExternalDoubleArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        PrintF(out, "  %d: %f\n", i, p->get_scalar(i));
      }
      break;
    }
    case DICTIONARY_ELEMENTS:
      elements()->Print(out);
      break;
    case NON_STRICT_ARGUMENTS_ELEMENTS: {
      FixedArray* p = FixedArray::cast(elements());
      for (int i = 2; i < p->length(); i++) {
        PrintF(out, "   %d: ", i);
        p->get(i)->ShortPrint(out);
        PrintF(out, "\n");
      }
      break;
    }
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
    case EXTERNAL_PIXEL_ARRAY_TYPE: return "EXTERNAL_PIXEL_ARRAY";
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
    case EXTERNAL_DOUBLE_ARRAY_TYPE: return "EXTERNAL_DOUBLE_ARRAY";
    case FILLER_TYPE: return "FILLER";
    case JS_OBJECT_TYPE: return "JS_OBJECT";
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE: return "JS_CONTEXT_EXTENSION_OBJECT";
    case ODDBALL_TYPE: return "ODDBALL";
    case JS_GLOBAL_PROPERTY_CELL_TYPE: return "JS_GLOBAL_PROPERTY_CELL";
    case SHARED_FUNCTION_INFO_TYPE: return "SHARED_FUNCTION_INFO";
    case JS_FUNCTION_TYPE: return "JS_FUNCTION";
    case CODE_TYPE: return "CODE";
    case JS_ARRAY_TYPE: return "JS_ARRAY";
    case JS_PROXY_TYPE: return "JS_PROXY";
    case JS_WEAK_MAP_TYPE: return "JS_WEAK_MAP";
    case JS_REGEXP_TYPE: return "JS_REGEXP";
    case JS_VALUE_TYPE: return "JS_VALUE";
    case JS_GLOBAL_OBJECT_TYPE: return "JS_GLOBAL_OBJECT";
    case JS_BUILTINS_OBJECT_TYPE: return "JS_BUILTINS_OBJECT";
    case JS_GLOBAL_PROXY_TYPE: return "JS_GLOBAL_PROXY";
    case FOREIGN_TYPE: return "FOREIGN";
    case JS_MESSAGE_OBJECT_TYPE: return "JS_MESSAGE_OBJECT_TYPE";
#define MAKE_STRUCT_CASE(NAME, Name, name) case NAME##_TYPE: return #NAME;
  STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    default: return "UNKNOWN";
  }
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


void CodeCache::CodeCachePrint(FILE* out) {
  HeapObject::PrintHeader(out, "CodeCache");
  PrintF(out, "\n - default_cache: ");
  default_cache()->ShortPrint(out);
  PrintF(out, "\n - normal_type_cache: ");
  normal_type_cache()->ShortPrint(out);
}


void PolymorphicCodeCache::PolymorphicCodeCachePrint(FILE* out) {
  HeapObject::PrintHeader(out, "PolymorphicCodeCache");
  PrintF(out, "\n - cache: ");
  cache()->ShortPrint(out);
}


void FixedArray::FixedArrayPrint(FILE* out) {
  HeapObject::PrintHeader(out, "FixedArray");
  PrintF(out, " - length: %d", length());
  for (int i = 0; i < length(); i++) {
    PrintF(out, "\n  [%d]: ", i);
    get(i)->ShortPrint(out);
  }
  PrintF(out, "\n");
}


void JSValue::JSValuePrint(FILE* out) {
  HeapObject::PrintHeader(out, "ValueObject");
  value()->Print(out);
}


void JSMessageObject::JSMessageObjectPrint(FILE* out) {
  HeapObject::PrintHeader(out, "JSMessageObject");
  PrintF(out, " - type: ");
  type()->ShortPrint(out);
  PrintF(out, "\n - arguments: ");
  arguments()->ShortPrint(out);
  PrintF(out, "\n - start_position: %d", start_position());
  PrintF(out, "\n - end_position: %d", end_position());
  PrintF(out, "\n - script: ");
  script()->ShortPrint(out);
  PrintF(out, "\n - stack_trace: ");
  stack_trace()->ShortPrint(out);
  PrintF(out, "\n - stack_frames: ");
  stack_frames()->ShortPrint(out);
  PrintF(out, "\n");
}


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


// This method is only meant to be called from gdb for debugging purposes.
// Since the string can also be in two-byte encoding, non-ascii characters
// will be ignored in the output.
char* String::ToAsciiArray() {
  // Static so that subsequent calls frees previously allocated space.
  // This also means that previous results will be overwritten.
  static char* buffer = NULL;
  if (buffer != NULL) free(buffer);
  buffer = new char[length()+1];
  WriteToFlat(this, buffer, 0, length());
  buffer[length()] = 0;
  return buffer;
}


void JSProxy::JSProxyPrint(FILE* out) {
  HeapObject::PrintHeader(out, "JSProxy");
  PrintF(out, " - map = 0x%p\n", reinterpret_cast<void*>(map()));
  PrintF(out, " - handler = ");
  handler()->Print(out);
  PrintF(out, "\n");
}


void JSFunctionProxy::JSFunctionProxyPrint(FILE* out) {
  HeapObject::PrintHeader(out, "JSFunctionProxy");
  PrintF(out, " - map = 0x%p\n", reinterpret_cast<void*>(map()));
  PrintF(out, " - handler = ");
  handler()->Print(out);
  PrintF(out, " - call_trap = ");
  call_trap()->Print(out);
  PrintF(out, " - construct_trap = ");
  construct_trap()->Print(out);
  PrintF(out, "\n");
}


void JSWeakMap::JSWeakMapPrint(FILE* out) {
  HeapObject::PrintHeader(out, "JSWeakMap");
  PrintF(out, " - map = 0x%p\n", reinterpret_cast<void*>(map()));
  PrintF(out, " - number of elements = %d\n", table()->NumberOfElements());
  PrintF(out, " - table = ");
  table()->ShortPrint(out);
  PrintF(out, "\n");
}


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


void JSGlobalProxy::JSGlobalProxyPrint(FILE* out) {
  PrintF(out, "global_proxy");
  JSObjectPrint(out);
  PrintF(out, "context : ");
  context()->ShortPrint(out);
  PrintF(out, "\n");
}


void JSGlobalObject::JSGlobalObjectPrint(FILE* out) {
  PrintF(out, "global ");
  JSObjectPrint(out);
  PrintF(out, "global context : ");
  global_context()->ShortPrint(out);
  PrintF(out, "\n");
}


void JSBuiltinsObject::JSBuiltinsObjectPrint(FILE* out) {
  PrintF(out, "builtins ");
  JSObjectPrint(out);
}


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


void Foreign::ForeignPrint(FILE* out) {
  PrintF(out, "foreign address : %p", address());
}


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


void AccessCheckInfo::AccessCheckInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "AccessCheckInfo");
  PrintF(out, "\n - named_callback: ");
  named_callback()->ShortPrint(out);
  PrintF(out, "\n - indexed_callback: ");
  indexed_callback()->ShortPrint(out);
  PrintF(out, "\n - data: ");
  data()->ShortPrint(out);
}


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


void CallHandlerInfo::CallHandlerInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "CallHandlerInfo");
  PrintF(out, "\n - callback: ");
  callback()->ShortPrint(out);
  PrintF(out, "\n - data: ");
  data()->ShortPrint(out);
  PrintF(out, "\n - call_stub_cache: ");
}


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


void ObjectTemplateInfo::ObjectTemplateInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "ObjectTemplateInfo");
  PrintF(out, "\n - constructor: ");
  constructor()->ShortPrint(out);
  PrintF(out, "\n - internal_field_count: ");
  internal_field_count()->ShortPrint(out);
}


void SignatureInfo::SignatureInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "SignatureInfo");
  PrintF(out, "\n - receiver: ");
  receiver()->ShortPrint(out);
  PrintF(out, "\n - args: ");
  args()->ShortPrint(out);
}


void TypeSwitchInfo::TypeSwitchInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "TypeSwitchInfo");
  PrintF(out, "\n - types: ");
  types()->ShortPrint(out);
}


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


#ifdef ENABLE_DEBUGGER_SUPPORT
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


void BreakPointInfo::BreakPointInfoPrint(FILE* out) {
  HeapObject::PrintHeader(out, "BreakPointInfo");
  PrintF(out, "\n - code_position: %d", code_position()->value());
  PrintF(out, "\n - source_position: %d", source_position()->value());
  PrintF(out, "\n - statement_position: %d", statement_position()->value());
  PrintF(out, "\n - break_point_objects: ");
  break_point_objects()->ShortPrint(out);
}
#endif  // ENABLE_DEBUGGER_SUPPORT


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


} }  // namespace v8::internal

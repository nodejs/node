// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/heap/objects-visiting.h"
#include "src/jsregexp.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

#ifdef OBJECT_PRINT

void Object::Print() {
  OFStream os(stdout);
  this->Print(os);
  os << flush;
}


void Object::Print(OStream& os) {  // NOLINT
  if (IsSmi()) {
    Smi::cast(this)->SmiPrint(os);
  } else {
    HeapObject::cast(this)->HeapObjectPrint(os);
  }
}


void HeapObject::PrintHeader(OStream& os, const char* id) {  // NOLINT
  os << "" << reinterpret_cast<void*>(this) << ": [" << id << "]\n";
}


void HeapObject::HeapObjectPrint(OStream& os) {  // NOLINT
  InstanceType instance_type = map()->instance_type();

  HandleScope scope(GetIsolate());
  if (instance_type < FIRST_NONSTRING_TYPE) {
    String::cast(this)->StringPrint(os);
    return;
  }

  switch (instance_type) {
    case SYMBOL_TYPE:
      Symbol::cast(this)->SymbolPrint(os);
      break;
    case MAP_TYPE:
      Map::cast(this)->MapPrint(os);
      break;
    case HEAP_NUMBER_TYPE:
      HeapNumber::cast(this)->HeapNumberPrint(os);
      break;
    case MUTABLE_HEAP_NUMBER_TYPE:
      os << "<mutable ";
      HeapNumber::cast(this)->HeapNumberPrint(os);
      os << ">";
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      FixedDoubleArray::cast(this)->FixedDoubleArrayPrint(os);
      break;
    case CONSTANT_POOL_ARRAY_TYPE:
      ConstantPoolArray::cast(this)->ConstantPoolArrayPrint(os);
      break;
    case FIXED_ARRAY_TYPE:
      FixedArray::cast(this)->FixedArrayPrint(os);
      break;
    case BYTE_ARRAY_TYPE:
      ByteArray::cast(this)->ByteArrayPrint(os);
      break;
    case FREE_SPACE_TYPE:
      FreeSpace::cast(this)->FreeSpacePrint(os);
      break;

#define PRINT_EXTERNAL_ARRAY(Type, type, TYPE, ctype, size)            \
  case EXTERNAL_##TYPE##_ARRAY_TYPE:                                   \
    External##Type##Array::cast(this)->External##Type##ArrayPrint(os); \
    break;

     TYPED_ARRAYS(PRINT_EXTERNAL_ARRAY)
#undef PRINT_EXTERNAL_ARRAY

#define PRINT_FIXED_TYPED_ARRAY(Type, type, TYPE, ctype, size) \
  case Fixed##Type##Array::kInstanceType:                      \
    Fixed##Type##Array::cast(this)->FixedTypedArrayPrint(os);  \
    break;

    TYPED_ARRAYS(PRINT_FIXED_TYPED_ARRAY)
#undef PRINT_FIXED_TYPED_ARRAY

    case FILLER_TYPE:
      os << "filler";
      break;
    case JS_OBJECT_TYPE:  // fall through
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_ARRAY_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_REGEXP_TYPE:
      JSObject::cast(this)->JSObjectPrint(os);
      break;
    case ODDBALL_TYPE:
      Oddball::cast(this)->to_string()->Print(os);
      break;
    case JS_MODULE_TYPE:
      JSModule::cast(this)->JSModulePrint(os);
      break;
    case JS_FUNCTION_TYPE:
      JSFunction::cast(this)->JSFunctionPrint(os);
      break;
    case JS_GLOBAL_PROXY_TYPE:
      JSGlobalProxy::cast(this)->JSGlobalProxyPrint(os);
      break;
    case JS_GLOBAL_OBJECT_TYPE:
      JSGlobalObject::cast(this)->JSGlobalObjectPrint(os);
      break;
    case JS_BUILTINS_OBJECT_TYPE:
      JSBuiltinsObject::cast(this)->JSBuiltinsObjectPrint(os);
      break;
    case JS_VALUE_TYPE:
      os << "Value wrapper around:";
      JSValue::cast(this)->value()->Print(os);
      break;
    case JS_DATE_TYPE:
      JSDate::cast(this)->JSDatePrint(os);
      break;
    case CODE_TYPE:
      Code::cast(this)->CodePrint(os);
      break;
    case JS_PROXY_TYPE:
      JSProxy::cast(this)->JSProxyPrint(os);
      break;
    case JS_FUNCTION_PROXY_TYPE:
      JSFunctionProxy::cast(this)->JSFunctionProxyPrint(os);
      break;
    case JS_SET_TYPE:
      JSSet::cast(this)->JSSetPrint(os);
      break;
    case JS_MAP_TYPE:
      JSMap::cast(this)->JSMapPrint(os);
      break;
    case JS_SET_ITERATOR_TYPE:
      JSSetIterator::cast(this)->JSSetIteratorPrint(os);
      break;
    case JS_MAP_ITERATOR_TYPE:
      JSMapIterator::cast(this)->JSMapIteratorPrint(os);
      break;
    case JS_WEAK_MAP_TYPE:
      JSWeakMap::cast(this)->JSWeakMapPrint(os);
      break;
    case JS_WEAK_SET_TYPE:
      JSWeakSet::cast(this)->JSWeakSetPrint(os);
      break;
    case FOREIGN_TYPE:
      Foreign::cast(this)->ForeignPrint(os);
      break;
    case SHARED_FUNCTION_INFO_TYPE:
      SharedFunctionInfo::cast(this)->SharedFunctionInfoPrint(os);
      break;
    case JS_MESSAGE_OBJECT_TYPE:
      JSMessageObject::cast(this)->JSMessageObjectPrint(os);
      break;
    case CELL_TYPE:
      Cell::cast(this)->CellPrint(os);
      break;
    case PROPERTY_CELL_TYPE:
      PropertyCell::cast(this)->PropertyCellPrint(os);
      break;
    case JS_ARRAY_BUFFER_TYPE:
      JSArrayBuffer::cast(this)->JSArrayBufferPrint(os);
      break;
    case JS_TYPED_ARRAY_TYPE:
      JSTypedArray::cast(this)->JSTypedArrayPrint(os);
      break;
    case JS_DATA_VIEW_TYPE:
      JSDataView::cast(this)->JSDataViewPrint(os);
      break;
#define MAKE_STRUCT_CASE(NAME, Name, name) \
  case NAME##_TYPE:                        \
    Name::cast(this)->Name##Print(os);     \
    break;
  STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE

    default:
      os << "UNKNOWN TYPE " << map()->instance_type();
      UNREACHABLE();
      break;
  }
}


void ByteArray::ByteArrayPrint(OStream& os) {  // NOLINT
  os << "byte array, data starts at " << GetDataStartAddress();
}


void FreeSpace::FreeSpacePrint(OStream& os) {  // NOLINT
  os << "free space, size " << Size();
}


#define EXTERNAL_ARRAY_PRINTER(Type, type, TYPE, ctype, size)           \
  void External##Type##Array::External##Type##ArrayPrint(OStream& os) { \
    os << "external " #type " array";                                   \
  }

TYPED_ARRAYS(EXTERNAL_ARRAY_PRINTER)

#undef EXTERNAL_ARRAY_PRINTER


template <class Traits>
void FixedTypedArray<Traits>::FixedTypedArrayPrint(OStream& os) {  // NOLINT
  os << "fixed " << Traits::Designator();
}


void JSObject::PrintProperties(OStream& os) {  // NOLINT
  if (HasFastProperties()) {
    DescriptorArray* descs = map()->instance_descriptors();
    for (int i = 0; i < map()->NumberOfOwnDescriptors(); i++) {
      os << "   ";
      descs->GetKey(i)->NamePrint(os);
      os << ": ";
      switch (descs->GetType(i)) {
        case FIELD: {
          FieldIndex index = FieldIndex::ForDescriptor(map(), i);
          os << Brief(RawFastPropertyAt(index)) << " (field at offset "
             << index.property_index() << ")\n";
          break;
        }
        case CONSTANT:
          os << Brief(descs->GetConstant(i)) << " (constant)\n";
          break;
        case CALLBACKS:
          os << Brief(descs->GetCallbacksObject(i)) << " (callback)\n";
          break;
        case NORMAL:  // only in slow mode
        case HANDLER:  // only in lookup results, not in descriptors
        case INTERCEPTOR:  // only in lookup results, not in descriptors
        // There are no transitions in the descriptor array.
        case NONEXISTENT:
          UNREACHABLE();
          break;
      }
    }
  } else {
    property_dictionary()->Print(os);
  }
}


template <class T>
static void DoPrintElements(OStream& os, Object* object) {  // NOLINT
  T* p = T::cast(object);
  for (int i = 0; i < p->length(); i++) {
    os << "   " << i << ": " << p->get_scalar(i) << "\n";
  }
}


void JSObject::PrintElements(OStream& os) {  // NOLINT
  // Don't call GetElementsKind, its validation code can cause the printer to
  // fail when debugging.
  switch (map()->elements_kind()) {
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case FAST_ELEMENTS: {
      // Print in array notation for non-sparse arrays.
      FixedArray* p = FixedArray::cast(elements());
      for (int i = 0; i < p->length(); i++) {
        os << "   " << i << ": " << Brief(p->get(i)) << "\n";
      }
      break;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      // Print in array notation for non-sparse arrays.
      if (elements()->length() > 0) {
        FixedDoubleArray* p = FixedDoubleArray::cast(elements());
        for (int i = 0; i < p->length(); i++) {
          os << "   " << i << ": ";
          if (p->is_the_hole(i)) {
            os << "<the hole>";
          } else {
            os << p->get_scalar(i);
          }
          os << "\n";
        }
      }
      break;
    }


#define PRINT_ELEMENTS(Kind, Type)         \
  case Kind: {                             \
    DoPrintElements<Type>(os, elements()); \
    break;                                 \
  }

    PRINT_ELEMENTS(EXTERNAL_UINT8_CLAMPED_ELEMENTS, ExternalUint8ClampedArray)
    PRINT_ELEMENTS(EXTERNAL_INT8_ELEMENTS, ExternalInt8Array)
    PRINT_ELEMENTS(EXTERNAL_UINT8_ELEMENTS,
        ExternalUint8Array)
    PRINT_ELEMENTS(EXTERNAL_INT16_ELEMENTS, ExternalInt16Array)
    PRINT_ELEMENTS(EXTERNAL_UINT16_ELEMENTS,
        ExternalUint16Array)
    PRINT_ELEMENTS(EXTERNAL_INT32_ELEMENTS, ExternalInt32Array)
    PRINT_ELEMENTS(EXTERNAL_UINT32_ELEMENTS,
        ExternalUint32Array)
    PRINT_ELEMENTS(EXTERNAL_FLOAT32_ELEMENTS, ExternalFloat32Array)
    PRINT_ELEMENTS(EXTERNAL_FLOAT64_ELEMENTS, ExternalFloat64Array)

    PRINT_ELEMENTS(UINT8_ELEMENTS, FixedUint8Array)
    PRINT_ELEMENTS(UINT8_CLAMPED_ELEMENTS, FixedUint8ClampedArray)
    PRINT_ELEMENTS(INT8_ELEMENTS, FixedInt8Array)
    PRINT_ELEMENTS(UINT16_ELEMENTS, FixedUint16Array)
    PRINT_ELEMENTS(INT16_ELEMENTS, FixedInt16Array)
    PRINT_ELEMENTS(UINT32_ELEMENTS, FixedUint32Array)
    PRINT_ELEMENTS(INT32_ELEMENTS, FixedInt32Array)
    PRINT_ELEMENTS(FLOAT32_ELEMENTS, FixedFloat32Array)
    PRINT_ELEMENTS(FLOAT64_ELEMENTS, FixedFloat64Array)

#undef PRINT_ELEMENTS

    case DICTIONARY_ELEMENTS:
      elements()->Print(os);
      break;
    case SLOPPY_ARGUMENTS_ELEMENTS: {
      FixedArray* p = FixedArray::cast(elements());
      os << "   parameter map:";
      for (int i = 2; i < p->length(); i++) {
        os << " " << (i - 2) << ":" << Brief(p->get(i));
      }
      os << "\n   context: " << Brief(p->get(0))
         << "\n   arguments: " << Brief(p->get(1)) << "\n";
      break;
    }
  }
}


void JSObject::PrintTransitions(OStream& os) {  // NOLINT
  if (!map()->HasTransitionArray()) return;
  TransitionArray* transitions = map()->transitions();
  for (int i = 0; i < transitions->number_of_transitions(); i++) {
    Name* key = transitions->GetKey(i);
    os << "   ";
    key->NamePrint(os);
    os << ": ";
    if (key == GetHeap()->frozen_symbol()) {
      os << " (transition to frozen)\n";
    } else if (key == GetHeap()->elements_transition_symbol()) {
      os << " (transition to "
         << ElementsKindToString(transitions->GetTarget(i)->elements_kind())
         << ")\n";
    } else if (key == GetHeap()->observed_symbol()) {
      os << " (transition to Object.observe)\n";
    } else {
      switch (transitions->GetTargetDetails(i).type()) {
        case FIELD: {
          os << " (transition to field)\n";
          break;
        }
        case CONSTANT:
          os << " (transition to constant)\n";
          break;
        case CALLBACKS:
          os << " (transition to callback)\n";
          break;
        // Values below are never in the target descriptor array.
        case NORMAL:
        case HANDLER:
        case INTERCEPTOR:
        case NONEXISTENT:
          UNREACHABLE();
          break;
      }
    }
  }
}


void JSObject::JSObjectPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSObject");
  // Don't call GetElementsKind, its validation code can cause the printer to
  // fail when debugging.
  PrototypeIterator iter(GetIsolate(), this);
  os << " - map = " << reinterpret_cast<void*>(map()) << " ["
     << ElementsKindToString(this->map()->elements_kind())
     << "]\n - prototype = " << reinterpret_cast<void*>(iter.GetCurrent())
     << "\n {\n";
  PrintProperties(os);
  PrintTransitions(os);
  PrintElements(os);
  os << " }\n";
}


void JSModule::JSModulePrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSModule");
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n"
     << " - context = ";
  context()->Print(os);
  os << " - scope_info = " << Brief(scope_info())
     << ElementsKindToString(this->map()->elements_kind()) << " {\n";
  PrintProperties(os);
  PrintElements(os);
  os << " }\n";
}


static const char* TypeToString(InstanceType type) {
  switch (type) {
#define TYPE_TO_STRING(TYPE) case TYPE: return #TYPE;
  INSTANCE_TYPE_LIST(TYPE_TO_STRING)
#undef TYPE_TO_STRING
  }
  UNREACHABLE();
  return "UNKNOWN";  // Keep the compiler happy.
}


void Symbol::SymbolPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Symbol");
  os << " - hash: " << Hash();
  os << "\n - name: " << Brief(name());
  os << " - private: " << is_private();
  os << "\n";
}


void Map::MapPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Map");
  os << " - type: " << TypeToString(instance_type()) << "\n";
  os << " - instance size: " << instance_size() << "\n";
  os << " - inobject properties: " << inobject_properties() << "\n";
  os << " - elements kind: " << ElementsKindToString(elements_kind());
  os << "\n - pre-allocated property fields: "
     << pre_allocated_property_fields() << "\n";
  os << " - unused property fields: " << unused_property_fields() << "\n";
  if (is_hidden_prototype()) os << " - hidden_prototype\n";
  if (has_named_interceptor()) os << " - named_interceptor\n";
  if (has_indexed_interceptor()) os << " - indexed_interceptor\n";
  if (is_undetectable()) os << " - undetectable\n";
  if (has_instance_call_handler()) os << " - instance_call_handler\n";
  if (is_access_check_needed()) os << " - access_check_needed\n";
  if (is_frozen()) {
    os << " - frozen\n";
  } else if (!is_extensible()) {
    os << " - sealed\n";
  }
  os << " - back pointer: " << Brief(GetBackPointer());
  os << "\n - instance descriptors " << (owns_descriptors() ? "(own) " : "")
     << "#" << NumberOfOwnDescriptors() << ": "
     << Brief(instance_descriptors());
  if (HasTransitionArray()) {
    os << "\n - transitions: " << Brief(transitions());
  }
  os << "\n - prototype: " << Brief(prototype());
  os << "\n - constructor: " << Brief(constructor());
  os << "\n - code cache: " << Brief(code_cache());
  os << "\n - dependent code: " << Brief(dependent_code());
  os << "\n";
}


void CodeCache::CodeCachePrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "CodeCache");
  os << "\n - default_cache: " << Brief(default_cache());
  os << "\n - normal_type_cache: " << Brief(normal_type_cache());
}


void PolymorphicCodeCache::PolymorphicCodeCachePrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PolymorphicCodeCache");
  os << "\n - cache: " << Brief(cache());
}


void TypeFeedbackInfo::TypeFeedbackInfoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "TypeFeedbackInfo");
  os << " - ic_total_count: " << ic_total_count()
     << ", ic_with_type_info_count: " << ic_with_type_info_count()
     << ", ic_generic_count: " << ic_generic_count() << "\n";
}


void AliasedArgumentsEntry::AliasedArgumentsEntryPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AliasedArgumentsEntry");
  os << "\n - aliased_context_slot: " << aliased_context_slot();
}


void FixedArray::FixedArrayPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "FixedArray");
  os << " - length: " << length();
  for (int i = 0; i < length(); i++) {
    os << "\n  [" << i << "]: " << Brief(get(i));
  }
  os << "\n";
}


void FixedDoubleArray::FixedDoubleArrayPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "FixedDoubleArray");
  os << " - length: " << length();
  for (int i = 0; i < length(); i++) {
    os << "\n  [" << i << "]: ";
    if (is_the_hole(i)) {
      os << "<the hole>";
    } else {
      os << get_scalar(i);
    }
  }
  os << "\n";
}


void ConstantPoolArray::ConstantPoolArrayPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "ConstantPoolArray");
  os << " - length: " << length();
  for (int i = 0; i <= last_index(INT32, SMALL_SECTION); i++) {
    if (i < last_index(INT64, SMALL_SECTION)) {
      os << "\n  [" << i << "]: double: " << get_int64_entry_as_double(i);
    } else if (i <= last_index(CODE_PTR, SMALL_SECTION)) {
      os << "\n  [" << i << "]: code target pointer: "
         << reinterpret_cast<void*>(get_code_ptr_entry(i));
    } else if (i <= last_index(HEAP_PTR, SMALL_SECTION)) {
      os << "\n  [" << i << "]: heap pointer: "
         << reinterpret_cast<void*>(get_heap_ptr_entry(i));
    } else if (i <= last_index(INT32, SMALL_SECTION)) {
      os << "\n  [" << i << "]: int32: " << get_int32_entry(i);
    }
  }
  if (is_extended_layout()) {
    os << "\n  Extended section:";
    for (int i = first_extended_section_index();
         i <= last_index(INT32, EXTENDED_SECTION); i++) {
      if (i < last_index(INT64, EXTENDED_SECTION)) {
        os << "\n  [" << i << "]: double: " << get_int64_entry_as_double(i);
      } else if (i <= last_index(CODE_PTR, EXTENDED_SECTION)) {
        os << "\n  [" << i << "]: code target pointer: "
           << reinterpret_cast<void*>(get_code_ptr_entry(i));
      } else if (i <= last_index(HEAP_PTR, EXTENDED_SECTION)) {
        os << "\n  [" << i << "]: heap pointer: "
           << reinterpret_cast<void*>(get_heap_ptr_entry(i));
      } else if (i <= last_index(INT32, EXTENDED_SECTION)) {
        os << "\n  [" << i << "]: int32: " << get_int32_entry(i);
      }
    }
  }
  os << "\n";
}


void JSValue::JSValuePrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "ValueObject");
  value()->Print(os);
}


void JSMessageObject::JSMessageObjectPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSMessageObject");
  os << " - type: " << Brief(type());
  os << "\n - arguments: " << Brief(arguments());
  os << "\n - start_position: " << start_position();
  os << "\n - end_position: " << end_position();
  os << "\n - script: " << Brief(script());
  os << "\n - stack_frames: " << Brief(stack_frames());
  os << "\n";
}


void String::StringPrint(OStream& os) {  // NOLINT
  if (StringShape(this).IsInternalized()) {
    os << "#";
  } else if (StringShape(this).IsCons()) {
    os << "c\"";
  } else {
    os << "\"";
  }

  const char truncated_epilogue[] = "...<truncated>";
  int len = length();
  if (!FLAG_use_verbose_printer) {
    if (len > 100) {
      len = 100 - sizeof(truncated_epilogue);
    }
  }
  for (int i = 0; i < len; i++) {
    os << AsUC16(Get(i));
  }
  if (len != length()) {
    os << truncated_epilogue;
  }

  if (!StringShape(this).IsInternalized()) os << "\"";
}


void Name::NamePrint(OStream& os) {  // NOLINT
  if (IsString())
    String::cast(this)->StringPrint(os);
  else
    os << Brief(this);
}


// This method is only meant to be called from gdb for debugging purposes.
// Since the string can also be in two-byte encoding, non-ASCII characters
// will be ignored in the output.
char* String::ToAsciiArray() {
  // Static so that subsequent calls frees previously allocated space.
  // This also means that previous results will be overwritten.
  static char* buffer = NULL;
  if (buffer != NULL) free(buffer);
  buffer = new char[length()+1];
  WriteToFlat(this, reinterpret_cast<uint8_t*>(buffer), 0, length());
  buffer[length()] = 0;
  return buffer;
}


static const char* const weekdays[] = {
  "???", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};


void JSDate::JSDatePrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSDate");
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n";
  os << " - value = ";
  value()->Print(os);
  if (!year()->IsSmi()) {
    os << " - time = NaN\n";
  } else {
    // TODO(svenpanne) Add some basic formatting to our streams.
    Vector<char> buf = Vector<char>::New(100);
    SNPrintF(
        buf, " - time = %s %04d/%02d/%02d %02d:%02d:%02d\n",
        weekdays[weekday()->IsSmi() ? Smi::cast(weekday())->value() + 1 : 0],
        year()->IsSmi() ? Smi::cast(year())->value() : -1,
        month()->IsSmi() ? Smi::cast(month())->value() : -1,
        day()->IsSmi() ? Smi::cast(day())->value() : -1,
        hour()->IsSmi() ? Smi::cast(hour())->value() : -1,
        min()->IsSmi() ? Smi::cast(min())->value() : -1,
        sec()->IsSmi() ? Smi::cast(sec())->value() : -1);
    os << buf.start();
  }
}


void JSProxy::JSProxyPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSProxy");
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n";
  os << " - handler = ";
  handler()->Print(os);
  os << "\n - hash = ";
  hash()->Print(os);
  os << "\n";
}


void JSFunctionProxy::JSFunctionProxyPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSFunctionProxy");
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n";
  os << " - handler = ";
  handler()->Print(os);
  os << "\n - call_trap = ";
  call_trap()->Print(os);
  os << "\n - construct_trap = ";
  construct_trap()->Print(os);
  os << "\n";
}


void JSSet::JSSetPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSSet");
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n";
  os << " - table = " << Brief(table());
  os << "\n";
}


void JSMap::JSMapPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSMap");
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n";
  os << " - table = " << Brief(table());
  os << "\n";
}


template <class Derived, class TableType>
void OrderedHashTableIterator<
    Derived, TableType>::OrderedHashTableIteratorPrint(OStream& os) {  // NOLINT
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n";
  os << " - table = " << Brief(table());
  os << "\n - index = " << Brief(index());
  os << "\n - kind = " << Brief(kind());
  os << "\n";
}


template void OrderedHashTableIterator<
    JSSetIterator,
    OrderedHashSet>::OrderedHashTableIteratorPrint(OStream& os);  // NOLINT


template void OrderedHashTableIterator<
    JSMapIterator,
    OrderedHashMap>::OrderedHashTableIteratorPrint(OStream& os);  // NOLINT


void JSSetIterator::JSSetIteratorPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSSetIterator");
  OrderedHashTableIteratorPrint(os);
}


void JSMapIterator::JSMapIteratorPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSMapIterator");
  OrderedHashTableIteratorPrint(os);
}


void JSWeakMap::JSWeakMapPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSWeakMap");
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n";
  os << " - table = " << Brief(table());
  os << "\n";
}


void JSWeakSet::JSWeakSetPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSWeakSet");
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n";
  os << " - table = " << Brief(table());
  os << "\n";
}


void JSArrayBuffer::JSArrayBufferPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSArrayBuffer");
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n";
  os << " - backing_store = " << backing_store() << "\n";
  os << " - byte_length = " << Brief(byte_length());
  os << "\n";
}


void JSTypedArray::JSTypedArrayPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSTypedArray");
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n";
  os << " - buffer =" << Brief(buffer());
  os << "\n - byte_offset = " << Brief(byte_offset());
  os << "\n - byte_length = " << Brief(byte_length());
  os << "\n - length = " << Brief(length());
  os << "\n";
  PrintElements(os);
}


void JSDataView::JSDataViewPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSDataView");
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n";
  os << " - buffer =" << Brief(buffer());
  os << "\n - byte_offset = " << Brief(byte_offset());
  os << "\n - byte_length = " << Brief(byte_length());
  os << "\n";
}


void JSFunction::JSFunctionPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Function");
  os << " - map = " << reinterpret_cast<void*>(map()) << "\n";
  os << " - initial_map = ";
  if (has_initial_map()) os << Brief(initial_map());
  os << "\n - shared_info = " << Brief(shared());
  os << "\n   - name = " << Brief(shared()->name());
  os << "\n - context = " << Brief(context());
  if (shared()->bound()) {
    os << "\n - bindings = " << Brief(function_bindings());
  } else {
    os << "\n - literals = " << Brief(literals());
  }
  os << "\n - code = " << Brief(code());
  os << "\n";
  PrintProperties(os);
  PrintElements(os);
  os << "\n";
}


void SharedFunctionInfo::SharedFunctionInfoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "SharedFunctionInfo");
  os << " - name: " << Brief(name());
  os << "\n - expected_nof_properties: " << expected_nof_properties();
  os << "\n - ast_node_count: " << ast_node_count();
  os << "\n - instance class name = ";
  instance_class_name()->Print(os);
  os << "\n - code = " << Brief(code());
  if (HasSourceCode()) {
    os << "\n - source code = ";
    String* source = String::cast(Script::cast(script())->source());
    int start = start_position();
    int length = end_position() - start;
    SmartArrayPointer<char> source_string =
        source->ToCString(DISALLOW_NULLS,
                          FAST_STRING_TRAVERSAL,
                          start, length, NULL);
    os << source_string.get();
  }
  // Script files are often large, hard to read.
  // os << "\n - script =";
  // script()->Print(os);
  os << "\n - function token position = " << function_token_position();
  os << "\n - start position = " << start_position();
  os << "\n - end position = " << end_position();
  os << "\n - is expression = " << is_expression();
  os << "\n - debug info = " << Brief(debug_info());
  os << "\n - length = " << length();
  os << "\n - optimized_code_map = " << Brief(optimized_code_map());
  os << "\n - feedback_vector = ";
  feedback_vector()->FixedArrayPrint(os);
  os << "\n";
}


void JSGlobalProxy::JSGlobalProxyPrint(OStream& os) {  // NOLINT
  os << "global_proxy ";
  JSObjectPrint(os);
  os << "native context : " << Brief(native_context());
  os << "\n";
}


void JSGlobalObject::JSGlobalObjectPrint(OStream& os) {  // NOLINT
  os << "global ";
  JSObjectPrint(os);
  os << "native context : " << Brief(native_context());
  os << "\n";
}


void JSBuiltinsObject::JSBuiltinsObjectPrint(OStream& os) {  // NOLINT
  os << "builtins ";
  JSObjectPrint(os);
}


void Cell::CellPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Cell");
}


void PropertyCell::PropertyCellPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PropertyCell");
}


void Code::CodePrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Code");
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_use_verbose_printer) {
    Disassemble(NULL, os);
  }
#endif
}


void Foreign::ForeignPrint(OStream& os) {  // NOLINT
  os << "foreign address : " << foreign_address();
}


void ExecutableAccessorInfo::ExecutableAccessorInfoPrint(
    OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "ExecutableAccessorInfo");
  os << "\n - name: " << Brief(name());
  os << "\n - flag: " << Brief(flag());
  os << "\n - getter: " << Brief(getter());
  os << "\n - setter: " << Brief(setter());
  os << "\n - data: " << Brief(data());
  os << "\n";
}


void DeclaredAccessorInfo::DeclaredAccessorInfoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "DeclaredAccessorInfo");
  os << "\n - name: " << Brief(name());
  os << "\n - flag: " << Brief(flag());
  os << "\n - descriptor: " << Brief(descriptor());
  os << "\n";
}


void DeclaredAccessorDescriptor::DeclaredAccessorDescriptorPrint(
    OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "DeclaredAccessorDescriptor");
  os << "\n - internal field: " << Brief(serialized_data());
  os << "\n";
}


void Box::BoxPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Box");
  os << "\n - value: " << Brief(value());
  os << "\n";
}


void AccessorPair::AccessorPairPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AccessorPair");
  os << "\n - getter: " << Brief(getter());
  os << "\n - setter: " << Brief(setter());
  os << "\n";
}


void AccessCheckInfo::AccessCheckInfoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AccessCheckInfo");
  os << "\n - named_callback: " << Brief(named_callback());
  os << "\n - indexed_callback: " << Brief(indexed_callback());
  os << "\n - data: " << Brief(data());
  os << "\n";
}


void InterceptorInfo::InterceptorInfoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "InterceptorInfo");
  os << "\n - getter: " << Brief(getter());
  os << "\n - setter: " << Brief(setter());
  os << "\n - query: " << Brief(query());
  os << "\n - deleter: " << Brief(deleter());
  os << "\n - enumerator: " << Brief(enumerator());
  os << "\n - data: " << Brief(data());
  os << "\n";
}


void CallHandlerInfo::CallHandlerInfoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "CallHandlerInfo");
  os << "\n - callback: " << Brief(callback());
  os << "\n - data: " << Brief(data());
  os << "\n";
}


void FunctionTemplateInfo::FunctionTemplateInfoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "FunctionTemplateInfo");
  os << "\n - class name: " << Brief(class_name());
  os << "\n - tag: " << Brief(tag());
  os << "\n - property_list: " << Brief(property_list());
  os << "\n - serial_number: " << Brief(serial_number());
  os << "\n - call_code: " << Brief(call_code());
  os << "\n - property_accessors: " << Brief(property_accessors());
  os << "\n - prototype_template: " << Brief(prototype_template());
  os << "\n - parent_template: " << Brief(parent_template());
  os << "\n - named_property_handler: " << Brief(named_property_handler());
  os << "\n - indexed_property_handler: " << Brief(indexed_property_handler());
  os << "\n - instance_template: " << Brief(instance_template());
  os << "\n - signature: " << Brief(signature());
  os << "\n - access_check_info: " << Brief(access_check_info());
  os << "\n - hidden_prototype: " << (hidden_prototype() ? "true" : "false");
  os << "\n - undetectable: " << (undetectable() ? "true" : "false");
  os << "\n - need_access_check: " << (needs_access_check() ? "true" : "false");
  os << "\n";
}


void ObjectTemplateInfo::ObjectTemplateInfoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "ObjectTemplateInfo");
  os << " - tag: " << Brief(tag());
  os << "\n - property_list: " << Brief(property_list());
  os << "\n - property_accessors: " << Brief(property_accessors());
  os << "\n - constructor: " << Brief(constructor());
  os << "\n - internal_field_count: " << Brief(internal_field_count());
  os << "\n";
}


void SignatureInfo::SignatureInfoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "SignatureInfo");
  os << "\n - receiver: " << Brief(receiver());
  os << "\n - args: " << Brief(args());
  os << "\n";
}


void TypeSwitchInfo::TypeSwitchInfoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "TypeSwitchInfo");
  os << "\n - types: " << Brief(types());
  os << "\n";
}


void AllocationSite::AllocationSitePrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AllocationSite");
  os << " - weak_next: " << Brief(weak_next());
  os << "\n - dependent code: " << Brief(dependent_code());
  os << "\n - nested site: " << Brief(nested_site());
  os << "\n - memento found count: "
     << Brief(Smi::FromInt(memento_found_count()));
  os << "\n - memento create count: "
     << Brief(Smi::FromInt(memento_create_count()));
  os << "\n - pretenure decision: "
     << Brief(Smi::FromInt(pretenure_decision()));
  os << "\n - transition_info: ";
  if (transition_info()->IsSmi()) {
    ElementsKind kind = GetElementsKind();
    os << "Array allocation with ElementsKind " << ElementsKindToString(kind);
  } else if (transition_info()->IsJSArray()) {
    os << "Array literal " << Brief(transition_info());
  } else {
    os << "unknown transition_info" << Brief(transition_info());
  }
  os << "\n";
}


void AllocationMemento::AllocationMementoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AllocationMemento");
  os << " - allocation site: ";
  if (IsValid()) {
    GetAllocationSite()->Print(os);
  } else {
    os << "<invalid>\n";
  }
}


void Script::ScriptPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Script");
  os << "\n - source: " << Brief(source());
  os << "\n - name: " << Brief(name());
  os << "\n - line_offset: " << Brief(line_offset());
  os << "\n - column_offset: " << Brief(column_offset());
  os << "\n - type: " << Brief(type());
  os << "\n - id: " << Brief(id());
  os << "\n - context data: " << Brief(context_data());
  os << "\n - wrapper: " << Brief(wrapper());
  os << "\n - compilation type: " << compilation_type();
  os << "\n - line ends: " << Brief(line_ends());
  os << "\n - eval from shared: " << Brief(eval_from_shared());
  os << "\n - eval from instructions offset: "
     << Brief(eval_from_instructions_offset());
  os << "\n";
}


void DebugInfo::DebugInfoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "DebugInfo");
  os << "\n - shared: " << Brief(shared());
  os << "\n - original_code: " << Brief(original_code());
  os << "\n - code: " << Brief(code());
  os << "\n - break_points: ";
  break_points()->Print(os);
}


void BreakPointInfo::BreakPointInfoPrint(OStream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "BreakPointInfo");
  os << "\n - code_position: " << code_position()->value();
  os << "\n - source_position: " << source_position()->value();
  os << "\n - statement_position: " << statement_position()->value();
  os << "\n - break_point_objects: " << Brief(break_point_objects());
  os << "\n";
}


void DescriptorArray::PrintDescriptors(OStream& os) {  // NOLINT
  os << "Descriptor array  " << number_of_descriptors() << "\n";
  for (int i = 0; i < number_of_descriptors(); i++) {
    Descriptor desc;
    Get(i, &desc);
    os << " " << i << ": " << desc;
  }
  os << "\n";
}


void TransitionArray::PrintTransitions(OStream& os) {  // NOLINT
  os << "Transition array  %d\n", number_of_transitions();
  for (int i = 0; i < number_of_transitions(); i++) {
    os << " " << i << ": ";
    GetKey(i)->NamePrint(os);
    os << ": ";
    switch (GetTargetDetails(i).type()) {
      case FIELD: {
        os << " (transition to field)\n";
        break;
      }
      case CONSTANT:
        os << " (transition to constant)\n";
        break;
      case CALLBACKS:
        os << " (transition to callback)\n";
        break;
      // Values below are never in the target descriptor array.
      case NORMAL:
      case HANDLER:
      case INTERCEPTOR:
      case NONEXISTENT:
        UNREACHABLE();
        break;
    }
  }
  os << "\n";
}


#endif  // OBJECT_PRINT


} }  // namespace v8::internal

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects.h"

#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"
#include "src/regexp/jsregexp.h"

namespace v8 {
namespace internal {

#ifdef OBJECT_PRINT

void Object::Print() {
  OFStream os(stdout);
  this->Print(os);
  os << std::flush;
}


void Object::Print(std::ostream& os) {  // NOLINT
  if (IsSmi()) {
    Smi::cast(this)->SmiPrint(os);
  } else {
    HeapObject::cast(this)->HeapObjectPrint(os);
  }
}


void HeapObject::PrintHeader(std::ostream& os, const char* id) {  // NOLINT
  os << reinterpret_cast<void*>(this) << ": [" << id << "]\n";
}


void HeapObject::HeapObjectPrint(std::ostream& os) {  // NOLINT
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
    case SIMD128_VALUE_TYPE:
      Simd128Value::cast(this)->Simd128ValuePrint(os);
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      FixedDoubleArray::cast(this)->FixedDoubleArrayPrint(os);
      break;
    case FIXED_ARRAY_TYPE:
      FixedArray::cast(this)->FixedArrayPrint(os);
      break;
    case BYTE_ARRAY_TYPE:
      ByteArray::cast(this)->ByteArrayPrint(os);
      break;
    case BYTECODE_ARRAY_TYPE:
      BytecodeArray::cast(this)->BytecodeArrayPrint(os);
      break;
    case TRANSITION_ARRAY_TYPE:
      TransitionArray::cast(this)->TransitionArrayPrint(os);
      break;
    case FREE_SPACE_TYPE:
      FreeSpace::cast(this)->FreeSpacePrint(os);
      break;

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
    case JS_PROMISE_TYPE:
      JSObject::cast(this)->JSObjectPrint(os);
      break;
    case JS_REGEXP_TYPE:
      JSRegExp::cast(this)->JSRegExpPrint(os);
      break;
    case ODDBALL_TYPE:
      Oddball::cast(this)->to_string()->Print(os);
      break;
    case JS_MODULE_TYPE:
      JSModule::cast(this)->JSModulePrint(os);
      break;
    case JS_BOUND_FUNCTION_TYPE:
      JSBoundFunction::cast(this)->JSBoundFunctionPrint(os);
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
    case JS_VALUE_TYPE:
      JSValue::cast(this)->JSValuePrint(os);
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
    case JS_ITERATOR_RESULT_TYPE:
      JSIteratorResult::cast(this)->JSIteratorResultPrint(os);
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
    case WEAK_CELL_TYPE:
      WeakCell::cast(this)->WeakCellPrint(os);
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


void Simd128Value::Simd128ValuePrint(std::ostream& os) {  // NOLINT
#define PRINT_SIMD128_VALUE(TYPE, Type, type, lane_count, lane_type) \
  if (Is##Type()) return Type::cast(this)->Type##Print(os);
  SIMD128_TYPES(PRINT_SIMD128_VALUE)
#undef PRINT_SIMD128_VALUE
  UNREACHABLE();
}


void Float32x4::Float32x4Print(std::ostream& os) {  // NOLINT
  char arr[100];
  Vector<char> buffer(arr, arraysize(arr));
  os << std::string(DoubleToCString(get_lane(0), buffer)) << ", "
     << std::string(DoubleToCString(get_lane(1), buffer)) << ", "
     << std::string(DoubleToCString(get_lane(2), buffer)) << ", "
     << std::string(DoubleToCString(get_lane(3), buffer));
}


#define SIMD128_INT_PRINT_FUNCTION(type, lane_count)                \
  void type::type##Print(std::ostream& os) {                        \
    char arr[100];                                                  \
    Vector<char> buffer(arr, arraysize(arr));                       \
    os << std::string(IntToCString(get_lane(0), buffer));           \
    for (int i = 1; i < lane_count; i++) {                          \
      os << ", " << std::string(IntToCString(get_lane(i), buffer)); \
    }                                                               \
  }
SIMD128_INT_PRINT_FUNCTION(Int32x4, 4)
SIMD128_INT_PRINT_FUNCTION(Uint32x4, 4)
SIMD128_INT_PRINT_FUNCTION(Int16x8, 8)
SIMD128_INT_PRINT_FUNCTION(Uint16x8, 8)
SIMD128_INT_PRINT_FUNCTION(Int8x16, 16)
SIMD128_INT_PRINT_FUNCTION(Uint8x16, 16)
#undef SIMD128_INT_PRINT_FUNCTION


#define SIMD128_BOOL_PRINT_FUNCTION(type, lane_count)            \
  void type::type##Print(std::ostream& os) {                     \
    char arr[100];                                               \
    Vector<char> buffer(arr, arraysize(arr));                    \
    os << std::string(get_lane(0) ? "true" : "false");           \
    for (int i = 1; i < lane_count; i++) {                       \
      os << ", " << std::string(get_lane(i) ? "true" : "false"); \
    }                                                            \
  }
SIMD128_BOOL_PRINT_FUNCTION(Bool32x4, 4)
SIMD128_BOOL_PRINT_FUNCTION(Bool16x8, 8)
SIMD128_BOOL_PRINT_FUNCTION(Bool8x16, 16)
#undef SIMD128_BOOL_PRINT_FUNCTION


void ByteArray::ByteArrayPrint(std::ostream& os) {  // NOLINT
  os << "byte array, data starts at " << GetDataStartAddress();
}


void BytecodeArray::BytecodeArrayPrint(std::ostream& os) {  // NOLINT
  Disassemble(os);
}


void FreeSpace::FreeSpacePrint(std::ostream& os) {  // NOLINT
  os << "free space, size " << Size();
}


template <class Traits>
void FixedTypedArray<Traits>::FixedTypedArrayPrint(
    std::ostream& os) {  // NOLINT
  os << "fixed " << Traits::Designator();
}


void JSObject::PrintProperties(std::ostream& os) {  // NOLINT
  if (HasFastProperties()) {
    DescriptorArray* descs = map()->instance_descriptors();
    for (int i = 0; i < map()->NumberOfOwnDescriptors(); i++) {
      os << "\n   ";
      descs->GetKey(i)->NamePrint(os);
      os << ": ";
      switch (descs->GetType(i)) {
        case DATA: {
          FieldIndex index = FieldIndex::ForDescriptor(map(), i);
          if (IsUnboxedDoubleField(index)) {
            os << "<unboxed double> " << RawFastDoublePropertyAt(index);
          } else {
            os << Brief(RawFastPropertyAt(index));
          }
          os << " (data field at offset " << index.property_index() << ")";
          break;
        }
        case ACCESSOR: {
          FieldIndex index = FieldIndex::ForDescriptor(map(), i);
          os << " (accessor field at offset " << index.property_index() << ")";
          break;
        }
        case DATA_CONSTANT:
          os << Brief(descs->GetConstant(i)) << " (data constant)";
          break;
        case ACCESSOR_CONSTANT:
          os << Brief(descs->GetCallbacksObject(i)) << " (accessor constant)";
          break;
      }
    }
  } else if (IsJSGlobalObject()) {
    global_dictionary()->Print(os);
  } else {
    property_dictionary()->Print(os);
  }
}


template <class T>
static void DoPrintElements(std::ostream& os, Object* object) {  // NOLINT
  T* p = T::cast(object);
  for (int i = 0; i < p->length(); i++) {
    os << "\n   " << i << ": " << p->get_scalar(i);
  }
}


void JSObject::PrintElements(std::ostream& os) {  // NOLINT
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
        os << "\n   " << i << ": " << Brief(p->get(i));
      }
      break;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      // Print in array notation for non-sparse arrays.
      if (elements()->length() > 0) {
        FixedDoubleArray* p = FixedDoubleArray::cast(elements());
        for (int i = 0; i < p->length(); i++) {
          os << "\n   " << i << ": ";
          if (p->is_the_hole(i)) {
            os << "<the hole>";
          } else {
            os << p->get_scalar(i);
          }
        }
      }
      break;
    }


#define PRINT_ELEMENTS(Kind, Type)         \
  case Kind: {                             \
    DoPrintElements<Type>(os, elements()); \
    break;                                 \
  }

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
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS: {
      FixedArray* p = FixedArray::cast(elements());
      os << "\n   parameter map:";
      for (int i = 2; i < p->length(); i++) {
        os << " " << (i - 2) << ":" << Brief(p->get(i));
      }
      os << "\n   context: " << Brief(p->get(0))
         << "\n   arguments: " << Brief(p->get(1));
      break;
    }
  }
}


static void JSObjectPrintHeader(std::ostream& os, JSObject* obj,
                                const char* id) {  // NOLINT
  obj->PrintHeader(os, id);
  // Don't call GetElementsKind, its validation code can cause the printer to
  // fail when debugging.
  PrototypeIterator iter(obj->GetIsolate(), obj);
  os << " - map = " << reinterpret_cast<void*>(obj->map()) << " ["
     << ElementsKindToString(obj->map()->elements_kind())
     << "]\n - prototype = " << reinterpret_cast<void*>(iter.GetCurrent());
}


static void JSObjectPrintBody(std::ostream& os, JSObject* obj,  // NOLINT
                              bool print_elements = true) {
  os << "\n {";
  obj->PrintProperties(os);
  obj->PrintTransitions(os);
  if (print_elements) obj->PrintElements(os);
  os << "\n }\n";
}


void JSObject::JSObjectPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSObject");
  JSObjectPrintBody(os, this);
}


void JSRegExp::JSRegExpPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSRegExp");
  os << "\n - data = " << Brief(data());
  JSObjectPrintBody(os, this);
}


void JSModule::JSModulePrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSModule");
  os << "\n - context = " << Brief(context());
  os << " - scope_info = " << Brief(scope_info());
  JSObjectPrintBody(os, this);
}


void Symbol::SymbolPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Symbol");
  os << " - hash: " << Hash();
  os << "\n - name: " << Brief(name());
  if (name()->IsUndefined()) {
    os << " (" << PrivateSymbolToName() << ")";
  }
  os << "\n - private: " << is_private();
  os << "\n";
}


void Map::MapPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Map");
  os << " - type: " << instance_type() << "\n";
  os << " - instance size: " << instance_size() << "\n";
  if (IsJSObjectMap()) {
    os << " - inobject properties: " << GetInObjectProperties() << "\n";
  }
  os << " - elements kind: " << ElementsKindToString(elements_kind()) << "\n";
  os << " - unused property fields: " << unused_property_fields() << "\n";
  if (is_deprecated()) os << " - deprecated_map\n";
  if (is_stable()) os << " - stable_map\n";
  if (is_dictionary_map()) os << " - dictionary_map\n";
  if (is_hidden_prototype()) os << " - hidden_prototype\n";
  if (has_named_interceptor()) os << " - named_interceptor\n";
  if (has_indexed_interceptor()) os << " - indexed_interceptor\n";
  if (is_undetectable()) os << " - undetectable\n";
  if (is_callable()) os << " - callable\n";
  if (is_constructor()) os << " - constructor\n";
  if (is_access_check_needed()) os << " - access_check_needed\n";
  if (!is_extensible()) os << " - non-extensible\n";
  if (is_observed()) os << " - observed\n";
  if (is_strong()) os << " - strong_map\n";
  if (is_prototype_map()) {
    os << " - prototype_map\n";
    os << " - prototype info: " << Brief(prototype_info());
  } else {
    os << " - back pointer: " << Brief(GetBackPointer());
  }
  os << "\n - instance descriptors " << (owns_descriptors() ? "(own) " : "")
     << "#" << NumberOfOwnDescriptors() << ": "
     << Brief(instance_descriptors());
  if (FLAG_unbox_double_fields) {
    os << "\n - layout descriptor: " << Brief(layout_descriptor());
  }
  int nof_transitions = TransitionArray::NumberOfTransitions(raw_transitions());
  if (nof_transitions > 0) {
    os << "\n - transitions #" << nof_transitions << ": "
       << Brief(raw_transitions());
    TransitionArray::PrintTransitions(os, raw_transitions(), false);
  }
  os << "\n - prototype: " << Brief(prototype());
  os << "\n - constructor: " << Brief(GetConstructor());
  os << "\n - code cache: " << Brief(code_cache());
  os << "\n - dependent code: " << Brief(dependent_code());
  os << "\n - construction counter: " << construction_counter();
  os << "\n";
}


void CodeCache::CodeCachePrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "CodeCache");
  os << "\n - default_cache: " << Brief(default_cache());
  os << "\n - normal_type_cache: " << Brief(normal_type_cache());
}


void PolymorphicCodeCache::PolymorphicCodeCachePrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PolymorphicCodeCache");
  os << "\n - cache: " << Brief(cache());
}


void TypeFeedbackInfo::TypeFeedbackInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "TypeFeedbackInfo");
  os << " - ic_total_count: " << ic_total_count()
     << ", ic_with_type_info_count: " << ic_with_type_info_count()
     << ", ic_generic_count: " << ic_generic_count() << "\n";
}


void AliasedArgumentsEntry::AliasedArgumentsEntryPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AliasedArgumentsEntry");
  os << "\n - aliased_context_slot: " << aliased_context_slot();
}


void FixedArray::FixedArrayPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "FixedArray");
  os << " - length: " << length();
  for (int i = 0; i < length(); i++) {
    os << "\n  [" << i << "]: " << Brief(get(i));
  }
  os << "\n";
}


void FixedDoubleArray::FixedDoubleArrayPrint(std::ostream& os) {  // NOLINT
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


void TransitionArray::TransitionArrayPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "TransitionArray");
  os << " - capacity: " << length();
  for (int i = 0; i < length(); i++) {
    os << "\n  [" << i << "]: " << Brief(get(i));
    if (i == kNextLinkIndex) os << " (next link)";
    if (i == kPrototypeTransitionsIndex) os << " (prototype transitions)";
    if (i == kTransitionLengthIndex) os << " (number of transitions)";
  }
  os << "\n";
}


void TypeFeedbackMetadata::Print() {
  OFStream os(stdout);
  TypeFeedbackMetadataPrint(os);
  os << std::flush;
}


void TypeFeedbackMetadata::TypeFeedbackMetadataPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "TypeFeedbackMetadata");
  os << " - length: " << length();
  if (length() == 0) {
    os << " (empty)\n";
    return;
  }

  TypeFeedbackMetadataIterator iter(this);
  while (iter.HasNext()) {
    FeedbackVectorSlot slot = iter.Next();
    FeedbackVectorSlotKind kind = iter.kind();
    os << "\n Slot " << slot << " " << kind;
  }
  os << "\n";
}


void TypeFeedbackVector::Print() {
  OFStream os(stdout);
  TypeFeedbackVectorPrint(os);
  os << std::flush;
}


void TypeFeedbackVector::TypeFeedbackVectorPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "TypeFeedbackVector");
  os << " - length: " << length();
  if (length() == 0) {
    os << " (empty)\n";
    return;
  }

  TypeFeedbackMetadataIterator iter(metadata());
  while (iter.HasNext()) {
    FeedbackVectorSlot slot = iter.Next();
    FeedbackVectorSlotKind kind = iter.kind();

    os << "\n Slot " << slot << " " << kind << " ";
    switch (kind) {
      case FeedbackVectorSlotKind::LOAD_IC: {
        LoadICNexus nexus(this, slot);
        os << Code::ICState2String(nexus.StateFromFeedback());
        break;
      }
      case FeedbackVectorSlotKind::KEYED_LOAD_IC: {
        KeyedLoadICNexus nexus(this, slot);
        os << Code::ICState2String(nexus.StateFromFeedback());
        break;
      }
      case FeedbackVectorSlotKind::CALL_IC: {
        CallICNexus nexus(this, slot);
        os << Code::ICState2String(nexus.StateFromFeedback());
        break;
      }
      case FeedbackVectorSlotKind::STORE_IC: {
        StoreICNexus nexus(this, slot);
        os << Code::ICState2String(nexus.StateFromFeedback());
        break;
      }
      case FeedbackVectorSlotKind::KEYED_STORE_IC: {
        KeyedStoreICNexus nexus(this, slot);
        os << Code::ICState2String(nexus.StateFromFeedback());
        break;
      }
      case FeedbackVectorSlotKind::GENERAL:
        break;
      case FeedbackVectorSlotKind::INVALID:
      case FeedbackVectorSlotKind::KINDS_NUMBER:
        UNREACHABLE();
        break;
    }

    int entry_size = iter.entry_size();
    for (int i = 0; i < entry_size; i++) {
      int index = GetIndex(slot) + i;
      os << "\n  [" << index << "]: " << Brief(get(index));
    }
  }
  os << "\n";
}


void JSValue::JSValuePrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSValue");
  os << "\n - value = " << Brief(value());
  JSObjectPrintBody(os, this);
}


void JSMessageObject::JSMessageObjectPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSMessageObject");
  os << "\n - type: " << type();
  os << "\n - arguments: " << Brief(argument());
  os << "\n - start_position: " << start_position();
  os << "\n - end_position: " << end_position();
  os << "\n - script: " << Brief(script());
  os << "\n - stack_frames: " << Brief(stack_frames());
  JSObjectPrintBody(os, this);
}


void String::StringPrint(std::ostream& os) {  // NOLINT
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


void Name::NamePrint(std::ostream& os) {  // NOLINT
  if (IsString()) {
    String::cast(this)->StringPrint(os);
  } else if (IsSymbol()) {
    Symbol::cast(this)->name()->Print(os);
  } else {
    os << Brief(this);
  }
}


static const char* const weekdays[] = {
  "???", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};


void JSDate::JSDatePrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSDate");
  os << "\n - value = " << Brief(value());
  if (!year()->IsSmi()) {
    os << "\n - time = NaN\n";
  } else {
    // TODO(svenpanne) Add some basic formatting to our streams.
    ScopedVector<char> buf(100);
    SNPrintF(
        buf, "\n - time = %s %04d/%02d/%02d %02d:%02d:%02d\n",
        weekdays[weekday()->IsSmi() ? Smi::cast(weekday())->value() + 1 : 0],
        year()->IsSmi() ? Smi::cast(year())->value() : -1,
        month()->IsSmi() ? Smi::cast(month())->value() : -1,
        day()->IsSmi() ? Smi::cast(day())->value() : -1,
        hour()->IsSmi() ? Smi::cast(hour())->value() : -1,
        min()->IsSmi() ? Smi::cast(min())->value() : -1,
        sec()->IsSmi() ? Smi::cast(sec())->value() : -1);
    os << buf.start();
  }
  JSObjectPrintBody(os, this);
}


void JSProxy::JSProxyPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSProxy");
  os << " - map = " << reinterpret_cast<void*>(map());
  os << "\n - target = ";
  target()->ShortPrint(os);
  os << "\n - handler = ";
  handler()->ShortPrint(os);
  os << "\n - hash = ";
  hash()->ShortPrint(os);
  os << "\n";
}


void JSSet::JSSetPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSSet");
  os << " - table = " << Brief(table());
  JSObjectPrintBody(os, this);
}


void JSMap::JSMapPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSMap");
  os << " - table = " << Brief(table());
  JSObjectPrintBody(os, this);
}


template <class Derived, class TableType>
void
OrderedHashTableIterator<Derived, TableType>::OrderedHashTableIteratorPrint(
    std::ostream& os) {  // NOLINT
  os << "\n - table = " << Brief(table());
  os << "\n - index = " << Brief(index());
  os << "\n - kind = " << Brief(kind());
  os << "\n";
}


template void OrderedHashTableIterator<
    JSSetIterator,
    OrderedHashSet>::OrderedHashTableIteratorPrint(std::ostream& os);  // NOLINT


template void OrderedHashTableIterator<
    JSMapIterator,
    OrderedHashMap>::OrderedHashTableIteratorPrint(std::ostream& os);  // NOLINT


void JSSetIterator::JSSetIteratorPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSSetIterator");
  OrderedHashTableIteratorPrint(os);
}


void JSMapIterator::JSMapIteratorPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSMapIterator");
  OrderedHashTableIteratorPrint(os);
}


void JSIteratorResult::JSIteratorResultPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSIteratorResult");
  os << "\n - done = " << Brief(done());
  os << "\n - value = " << Brief(value());
  os << "\n";
}


void JSWeakMap::JSWeakMapPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSWeakMap");
  os << "\n - table = " << Brief(table());
  JSObjectPrintBody(os, this);
}


void JSWeakSet::JSWeakSetPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSWeakSet");
  os << "\n - table = " << Brief(table());
  JSObjectPrintBody(os, this);
}


void JSArrayBuffer::JSArrayBufferPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSArrayBuffer");
  os << "\n - backing_store = " << backing_store();
  os << "\n - byte_length = " << Brief(byte_length());
  if (was_neutered()) os << " - neutered\n";
  JSObjectPrintBody(os, this, !was_neutered());
}


void JSTypedArray::JSTypedArrayPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSTypedArray");
  os << "\n - buffer = " << Brief(buffer());
  os << "\n - byte_offset = " << Brief(byte_offset());
  os << "\n - byte_length = " << Brief(byte_length());
  os << "\n - length = " << Brief(length());
  if (WasNeutered()) os << " - neutered\n";
  JSObjectPrintBody(os, this, !WasNeutered());
}


void JSDataView::JSDataViewPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSDataView");
  os << "\n - buffer =" << Brief(buffer());
  os << "\n - byte_offset = " << Brief(byte_offset());
  os << "\n - byte_length = " << Brief(byte_length());
  if (WasNeutered()) os << " - neutered\n";
  JSObjectPrintBody(os, this, !WasNeutered());
}


void JSBoundFunction::JSBoundFunctionPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSBoundFunction");
  os << "\n - bound_target_function = " << Brief(bound_target_function());
  os << "\n - bound_this = " << Brief(bound_this());
  os << "\n - bound_arguments = " << Brief(bound_arguments());
  JSObjectPrintBody(os, this);
}


void JSFunction::JSFunctionPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "Function");
  os << "\n - initial_map = ";
  if (has_initial_map()) os << Brief(initial_map());
  os << "\n - shared_info = " << Brief(shared());
  os << "\n - name = " << Brief(shared()->name());
  if (shared()->is_generator()) {
    os << "\n   - generator";
  }
  os << "\n - context = " << Brief(context());
  os << "\n - literals = " << Brief(literals());
  os << "\n - code = " << Brief(code());
  JSObjectPrintBody(os, this);
}


void SharedFunctionInfo::SharedFunctionInfoPrint(std::ostream& os) {  // NOLINT
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
    base::SmartArrayPointer<char> source_string = source->ToCString(
        DISALLOW_NULLS, FAST_STRING_TRAVERSAL, start, length, NULL);
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
  feedback_vector()->TypeFeedbackVectorPrint(os);
  if (HasBytecodeArray()) {
    os << "\n - bytecode_array = " << bytecode_array();
  }
  os << "\n";
}


void JSGlobalProxy::JSGlobalProxyPrint(std::ostream& os) {  // NOLINT
  os << "global_proxy ";
  JSObjectPrint(os);
  os << "native context : " << Brief(native_context());
  os << "\n";
}


void JSGlobalObject::JSGlobalObjectPrint(std::ostream& os) {  // NOLINT
  os << "global ";
  JSObjectPrint(os);
  os << "native context : " << Brief(native_context());
  os << "\n";
}


void Cell::CellPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Cell");
  os << " - value: " << Brief(value());
  os << "\n";
}


void PropertyCell::PropertyCellPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PropertyCell");
  os << " - value: " << Brief(value());
  os << "\n - details: " << property_details();
  os << "\n";
}


void WeakCell::WeakCellPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "WeakCell");
  if (cleared()) {
    os << "\n - cleared";
  } else {
    os << "\n - value: " << Brief(value());
  }
  os << "\n";
}


void Code::CodePrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Code");
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_use_verbose_printer) {
    Disassemble(NULL, os);
  }
#endif
}


void Foreign::ForeignPrint(std::ostream& os) {  // NOLINT
  os << "foreign address : " << foreign_address();
  os << "\n";
}


void ExecutableAccessorInfo::ExecutableAccessorInfoPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "ExecutableAccessorInfo");
  os << "\n - name: " << Brief(name());
  os << "\n - flag: " << flag();
  os << "\n - getter: " << Brief(getter());
  os << "\n - setter: " << Brief(setter());
  os << "\n - data: " << Brief(data());
  os << "\n";
}


void Box::BoxPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Box");
  os << "\n - value: " << Brief(value());
  os << "\n";
}


void PrototypeInfo::PrototypeInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PrototypeInfo");
  os << "\n - prototype users: " << Brief(prototype_users());
  os << "\n - registry slot: " << registry_slot();
  os << "\n - validity cell: " << Brief(validity_cell());
  os << "\n";
}


void SloppyBlockWithEvalContextExtension::
    SloppyBlockWithEvalContextExtensionPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "SloppyBlockWithEvalContextExtension");
  os << "\n - scope_info: " << Brief(scope_info());
  os << "\n - extension: " << Brief(extension());
  os << "\n";
}


void AccessorPair::AccessorPairPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AccessorPair");
  os << "\n - getter: " << Brief(getter());
  os << "\n - setter: " << Brief(setter());
  os << "\n";
}


void AccessCheckInfo::AccessCheckInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AccessCheckInfo");
  os << "\n - named_callback: " << Brief(named_callback());
  os << "\n - indexed_callback: " << Brief(indexed_callback());
  os << "\n - callback: " << Brief(callback());
  os << "\n - data: " << Brief(data());
  os << "\n";
}


void InterceptorInfo::InterceptorInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "InterceptorInfo");
  os << "\n - getter: " << Brief(getter());
  os << "\n - setter: " << Brief(setter());
  os << "\n - query: " << Brief(query());
  os << "\n - deleter: " << Brief(deleter());
  os << "\n - enumerator: " << Brief(enumerator());
  os << "\n - data: " << Brief(data());
  os << "\n";
}


void CallHandlerInfo::CallHandlerInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "CallHandlerInfo");
  os << "\n - callback: " << Brief(callback());
  os << "\n - data: " << Brief(data());
  os << "\n";
}


void FunctionTemplateInfo::FunctionTemplateInfoPrint(
    std::ostream& os) {  // NOLINT
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
  os << "\n - instantiated: " << (instantiated() ? "true" : "false");
  os << "\n";
}


void ObjectTemplateInfo::ObjectTemplateInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "ObjectTemplateInfo");
  os << " - tag: " << Brief(tag());
  os << "\n - property_list: " << Brief(property_list());
  os << "\n - property_accessors: " << Brief(property_accessors());
  os << "\n - constructor: " << Brief(constructor());
  os << "\n - internal_field_count: " << Brief(internal_field_count());
  os << "\n";
}


void AllocationSite::AllocationSitePrint(std::ostream& os) {  // NOLINT
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


void AllocationMemento::AllocationMementoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AllocationMemento");
  os << " - allocation site: ";
  if (IsValid()) {
    GetAllocationSite()->Print(os);
  } else {
    os << "<invalid>\n";
  }
}


void Script::ScriptPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Script");
  os << "\n - source: " << Brief(source());
  os << "\n - name: " << Brief(name());
  os << "\n - line_offset: " << line_offset();
  os << "\n - column_offset: " << column_offset();
  os << "\n - type: " << type();
  os << "\n - id: " << id();
  os << "\n - context data: " << Brief(context_data());
  os << "\n - wrapper: " << Brief(wrapper());
  os << "\n - compilation type: " << compilation_type();
  os << "\n - line ends: " << Brief(line_ends());
  os << "\n - eval from shared: " << Brief(eval_from_shared());
  os << "\n - eval from instructions offset: "
     << eval_from_instructions_offset();
  os << "\n - shared function infos: " << Brief(shared_function_infos());
  os << "\n";
}


void DebugInfo::DebugInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "DebugInfo");
  os << "\n - shared: " << Brief(shared());
  os << "\n - code: " << Brief(code());
  os << "\n - break_points: ";
  break_points()->Print(os);
}


void BreakPointInfo::BreakPointInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "BreakPointInfo");
  os << "\n - code_position: " << code_position();
  os << "\n - source_position: " << source_position();
  os << "\n - statement_position: " << statement_position();
  os << "\n - break_point_objects: " << Brief(break_point_objects());
  os << "\n";
}


static void PrintBitMask(std::ostream& os, uint32_t value) {  // NOLINT
  for (int i = 0; i < 32; i++) {
    if ((i & 7) == 0) os << " ";
    os << (((value & 1) == 0) ? "_" : "x");
    value >>= 1;
  }
}


void LayoutDescriptor::Print() {
  OFStream os(stdout);
  this->Print(os);
  os << std::flush;
}


void LayoutDescriptor::Print(std::ostream& os) {  // NOLINT
  os << "Layout descriptor: ";
  if (IsUninitialized()) {
    os << "<uninitialized>";
  } else if (IsFastPointerLayout()) {
    os << "<all tagged>";
  } else if (IsSmi()) {
    os << "fast";
    PrintBitMask(os, static_cast<uint32_t>(Smi::cast(this)->value()));
  } else {
    os << "slow";
    int len = length();
    for (int i = 0; i < len; i++) {
      if (i > 0) os << " |";
      PrintBitMask(os, get_scalar(i));
    }
  }
  os << "\n";
}


#endif  // OBJECT_PRINT


#if TRACE_MAPS


void Name::NameShortPrint() {
  if (this->IsString()) {
    PrintF("%s", String::cast(this)->ToCString().get());
  } else {
    DCHECK(this->IsSymbol());
    Symbol* s = Symbol::cast(this);
    if (s->name()->IsUndefined()) {
      PrintF("#<%s>", s->PrivateSymbolToName());
    } else {
      PrintF("<%s>", String::cast(s->name())->ToCString().get());
    }
  }
}


int Name::NameShortPrint(Vector<char> str) {
  if (this->IsString()) {
    return SNPrintF(str, "%s", String::cast(this)->ToCString().get());
  } else {
    DCHECK(this->IsSymbol());
    Symbol* s = Symbol::cast(this);
    if (s->name()->IsUndefined()) {
      return SNPrintF(str, "#<%s>", s->PrivateSymbolToName());
    } else {
      return SNPrintF(str, "<%s>", String::cast(s->name())->ToCString().get());
    }
  }
}


#endif  // TRACE_MAPS


#if defined(DEBUG) || defined(OBJECT_PRINT)
// This method is only meant to be called from gdb for debugging purposes.
// Since the string can also be in two-byte encoding, non-Latin1 characters
// will be ignored in the output.
char* String::ToAsciiArray() {
  // Static so that subsequent calls frees previously allocated space.
  // This also means that previous results will be overwritten.
  static char* buffer = NULL;
  if (buffer != NULL) delete[] buffer;
  buffer = new char[length() + 1];
  WriteToFlat(this, reinterpret_cast<uint8_t*>(buffer), 0, length());
  buffer[length()] = 0;
  return buffer;
}


void DescriptorArray::Print() {
  OFStream os(stdout);
  this->PrintDescriptors(os);
  os << std::flush;
}


void DescriptorArray::PrintDescriptors(std::ostream& os) {  // NOLINT
  HandleScope scope(GetIsolate());
  os << "Descriptor array #" << number_of_descriptors();
  for (int i = 0; i < number_of_descriptors(); i++) {
    Descriptor desc;
    Get(i, &desc);
    os << "\n " << i << ": " << desc;
  }
  os << "\n";
}


void TransitionArray::Print() {
  OFStream os(stdout);
  TransitionArray::PrintTransitions(os, this);
  os << "\n" << std::flush;
}


void TransitionArray::PrintTransitions(std::ostream& os, Object* transitions,
                                       bool print_header) {  // NOLINT
  int num_transitions = NumberOfTransitions(transitions);
  if (print_header) {
    os << "Transition array #" << num_transitions << ":";
  }
  for (int i = 0; i < num_transitions; i++) {
    Name* key = GetKey(transitions, i);
    Map* target = GetTarget(transitions, i);
    os << "\n   ";
#ifdef OBJECT_PRINT
    key->NamePrint(os);
#else
    key->ShortPrint(os);
#endif
    os << ": ";
    Heap* heap = key->GetHeap();
    if (key == heap->nonextensible_symbol()) {
      os << "(transition to non-extensible)";
    } else if (key == heap->sealed_symbol()) {
      os << "(transition to sealed)";
    } else if (key == heap->frozen_symbol()) {
      os << "(transition to frozen)";
    } else if (key == heap->elements_transition_symbol()) {
      os << "(transition to " << ElementsKindToString(target->elements_kind())
         << ")";
    } else if (key == heap->strict_function_transition_symbol()) {
      os << " (transition to strict function)";
    } else if (key == heap->strong_function_transition_symbol()) {
      os << " (transition to strong function)";
    } else if (key == heap->observed_symbol()) {
      os << " (transition to Object.observe)";
    } else {
      PropertyDetails details = GetTargetDetails(key, target);
      os << "(transition to ";
      if (details.location() == kDescriptor) {
        os << "immutable ";
      }
      os << (details.kind() == kData ? "data" : "accessor");
      if (details.location() == kDescriptor) {
        Object* value =
            target->instance_descriptors()->GetValue(target->LastAdded());
        os << " " << Brief(value);
      }
      os << "), attrs: " << details.attributes();
    }
    os << " -> " << Brief(target);
  }
}


void JSObject::PrintTransitions(std::ostream& os) {  // NOLINT
  Object* transitions = map()->raw_transitions();
  int num_transitions = TransitionArray::NumberOfTransitions(transitions);
  if (num_transitions == 0) return;
  os << "\n - transitions";
  TransitionArray::PrintTransitions(os, transitions, false);
}
#endif  // defined(DEBUG) || defined(OBJECT_PRINT)
}  // namespace internal
}  // namespace v8

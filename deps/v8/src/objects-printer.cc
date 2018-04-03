// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects.h"

#include <iomanip>
#include <memory>

#include "src/bootstrapper.h"
#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/ostreams.h"
#include "src/regexp/jsregexp.h"
#include "src/transitions-inl.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects-inl.h"

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
    os << "Smi: " << std::hex << "0x" << Smi::ToInt(this);
    os << std::dec << " (" << Smi::ToInt(this) << ")\n";
  } else {
    HeapObject::cast(this)->HeapObjectPrint(os);
  }
}


void HeapObject::PrintHeader(std::ostream& os, const char* id) {  // NOLINT
  os << reinterpret_cast<void*>(this) << ": [";
  if (id != nullptr) {
    os << id;
  } else {
    os << map()->instance_type();
  }
  os << "]";
  if (GetHeap()->InOldSpace(this)) os << " in OldSpace";
}


void HeapObject::HeapObjectPrint(std::ostream& os) {  // NOLINT
  InstanceType instance_type = map()->instance_type();

  HandleScope scope(GetIsolate());
  if (instance_type < FIRST_NONSTRING_TYPE) {
    String::cast(this)->StringPrint(os);
    os << "\n";
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
      os << "\n";
      break;
    case MUTABLE_HEAP_NUMBER_TYPE:
      os << "<mutable ";
      HeapNumber::cast(this)->HeapNumberPrint(os);
      os << ">\n";
      break;
    case BIGINT_TYPE:
      BigInt::cast(this)->BigIntPrint(os);
      os << "\n";
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      FixedDoubleArray::cast(this)->FixedDoubleArrayPrint(os);
      break;
    case HASH_TABLE_TYPE:
    case FIXED_ARRAY_TYPE:
      FixedArray::cast(this)->FixedArrayPrint(os);
      break;
    case PROPERTY_ARRAY_TYPE:
      PropertyArray::cast(this)->PropertyArrayPrint(os);
      break;
    case BYTE_ARRAY_TYPE:
      ByteArray::cast(this)->ByteArrayPrint(os);
      break;
    case BYTECODE_ARRAY_TYPE:
      BytecodeArray::cast(this)->BytecodeArrayPrint(os);
      break;
    case DESCRIPTOR_ARRAY_TYPE:
      DescriptorArray::cast(this)->PrintDescriptors(os);
      break;
    case TRANSITION_ARRAY_TYPE:
      TransitionArray::cast(this)->TransitionArrayPrint(os);
      break;
    case FEEDBACK_VECTOR_TYPE:
      FeedbackVector::cast(this)->FeedbackVectorPrint(os);
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

#define ARRAY_ITERATOR_CASE(type) case type:
    ARRAY_ITERATOR_TYPE_LIST(ARRAY_ITERATOR_CASE)
#undef ARRAY_ITERATOR_CASE
    JSArrayIterator::cast(this)->JSArrayIteratorPrint(os);
    break;

    case FILLER_TYPE:
      os << "filler";
      break;
    case JS_OBJECT_TYPE:  // fall through
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_ARGUMENTS_TYPE:
    case JS_ERROR_TYPE:
    case WASM_INSTANCE_TYPE:  // TODO(titzer): debug printing for wasm objects
    case WASM_MEMORY_TYPE:
    case WASM_MODULE_TYPE:
    case WASM_TABLE_TYPE:
      JSObject::cast(this)->JSObjectPrint(os);
      break;
    case JS_PROMISE_TYPE:
      JSPromise::cast(this)->JSPromisePrint(os);
      break;
    case JS_ARRAY_TYPE:
      JSArray::cast(this)->JSArrayPrint(os);
      break;
    case JS_REGEXP_TYPE:
      JSRegExp::cast(this)->JSRegExpPrint(os);
      break;
    case ODDBALL_TYPE:
      Oddball::cast(this)->to_string()->Print(os);
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
    case CODE_DATA_CONTAINER_TYPE:
      CodeDataContainer::cast(this)->CodeDataContainerPrint(os);
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
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE:
      JSSetIterator::cast(this)->JSSetIteratorPrint(os);
      break;
    case JS_MAP_KEY_ITERATOR_TYPE:
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_MAP_VALUE_ITERATOR_TYPE:
      JSMapIterator::cast(this)->JSMapIteratorPrint(os);
      break;
    case JS_WEAK_MAP_TYPE:
      JSWeakMap::cast(this)->JSWeakMapPrint(os);
      break;
    case JS_WEAK_SET_TYPE:
      JSWeakSet::cast(this)->JSWeakSetPrint(os);
      break;
    case JS_MODULE_NAMESPACE_TYPE:
      JSModuleNamespace::cast(this)->JSModuleNamespacePrint(os);
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

    case LOAD_HANDLER_TYPE:
      LoadHandler::cast(this)->LoadHandlerPrint(os);
      break;

    case STORE_HANDLER_TYPE:
      StoreHandler::cast(this)->StoreHandlerPrint(os);
      break;

    default:
      os << "UNKNOWN TYPE " << map()->instance_type();
      UNREACHABLE();
      break;
  }
}

void ByteArray::ByteArrayPrint(std::ostream& os) {  // NOLINT
  os << "byte array, data starts at " << GetDataStartAddress();
}


void BytecodeArray::BytecodeArrayPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "BytecodeArray");
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

bool JSObject::PrintProperties(std::ostream& os) {  // NOLINT
  if (HasFastProperties()) {
    DescriptorArray* descs = map()->instance_descriptors();
    int nof_inobject_properties = map()->GetInObjectProperties();
    int i = 0;
    for (; i < map()->NumberOfOwnDescriptors(); i++) {
      os << "\n    ";
      descs->GetKey(i)->NamePrint(os);
      os << ": ";
      PropertyDetails details = descs->GetDetails(i);
      switch (details.location()) {
        case kField: {
          FieldIndex field_index = FieldIndex::ForDescriptor(map(), i);
          if (IsUnboxedDoubleField(field_index)) {
            os << "<unboxed double> " << RawFastDoublePropertyAt(field_index);
          } else {
            os << Brief(RawFastPropertyAt(field_index));
          }
          break;
        }
        case kDescriptor:
          os << Brief(descs->GetValue(i));
          break;
      }
      os << " ";
      details.PrintAsFastTo(os, PropertyDetails::kForProperties);
      if (details.location() != kField) continue;
      int field_index = details.field_index();
      if (nof_inobject_properties <= field_index) {
        field_index -= nof_inobject_properties;
        os << " properties[" << field_index << "]";
      }
    }
    return i > 0;
  } else if (IsJSGlobalObject()) {
    JSGlobalObject::cast(this)->global_dictionary()->Print(os);
  } else {
    property_dictionary()->Print(os);
  }
  return true;
}

namespace {

template <class T>
bool IsTheHoleAt(T* array, int index) {
  return false;
}

template <>
bool IsTheHoleAt(FixedDoubleArray* array, int index) {
  return array->is_the_hole(index);
}

template <class T>
double GetScalarElement(T* array, int index) {
  if (IsTheHoleAt(array, index)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return array->get_scalar(index);
}

template <class T>
void DoPrintElements(std::ostream& os, Object* object) {  // NOLINT
  const bool print_the_hole = std::is_same<T, FixedDoubleArray>::value;
  T* array = T::cast(object);
  if (array->length() == 0) return;
  int previous_index = 0;
  double previous_value = GetScalarElement(array, 0);
  double value = 0.0;
  int i;
  for (i = 1; i <= array->length(); i++) {
    if (i < array->length()) value = GetScalarElement(array, i);
    bool values_are_nan = std::isnan(previous_value) && std::isnan(value);
    if (i != array->length() && (previous_value == value || values_are_nan) &&
        IsTheHoleAt(array, i - 1) == IsTheHoleAt(array, i)) {
      continue;
    }
    os << "\n";
    std::stringstream ss;
    ss << previous_index;
    if (previous_index != i - 1) {
      ss << '-' << (i - 1);
    }
    os << std::setw(12) << ss.str() << ": ";
    if (print_the_hole && IsTheHoleAt(array, i - 1)) {
      os << "<the_hole>";
    } else {
      os << previous_value;
    }
    previous_index = i;
    previous_value = value;
  }
}

template <typename T>
void PrintFixedArrayElements(std::ostream& os, T* array) {
  // Print in array notation for non-sparse arrays.
  Object* previous_value = array->length() > 0 ? array->get(0) : nullptr;
  Object* value = nullptr;
  int previous_index = 0;
  int i;
  for (i = 1; i <= array->length(); i++) {
    if (i < array->length()) value = array->get(i);
    if (previous_value == value && i != array->length()) {
      continue;
    }
    os << "\n";
    std::stringstream ss;
    ss << previous_index;
    if (previous_index != i - 1) {
      ss << '-' << (i - 1);
    }
    os << std::setw(12) << ss.str() << ": " << Brief(previous_value);
    previous_index = i;
    previous_value = value;
  }
}

void PrintDictionaryElements(std::ostream& os, FixedArrayBase* elements) {
  // Print some internal fields
  NumberDictionary* dict = NumberDictionary::cast(elements);
  if (dict->requires_slow_elements()) {
    os << "\n   - requires_slow_elements";
  } else {
    os << "\n   - max_number_key: " << dict->max_number_key();
  }
  dict->Print(os);
}

void PrintSloppyArgumentElements(std::ostream& os, ElementsKind kind,
                                 SloppyArgumentsElements* elements) {
  Isolate* isolate = elements->GetIsolate();
  FixedArray* arguments_store = elements->arguments();
  os << "\n    0: context = " << Brief(elements->context())
     << "\n    1: arguments_store = " << Brief(arguments_store)
     << "\n    parameter to context slot map:";
  for (uint32_t i = 0; i < elements->parameter_map_length(); i++) {
    uint32_t raw_index = i + SloppyArgumentsElements::kParameterMapStart;
    Object* mapped_entry = elements->get_mapped_entry(i);
    os << "\n    " << raw_index << ": param(" << i
       << ") = " << Brief(mapped_entry);
    if (mapped_entry->IsTheHole(isolate)) {
      os << " in the arguments_store[" << i << "]";
    } else {
      os << " in the context";
    }
  }
  if (arguments_store->length() == 0) return;
  os << "\n }"
     << "\n - arguments_store = " << Brief(arguments_store) << " "
     << ElementsKindToString(arguments_store->map()->elements_kind()) << " {";
  if (kind == FAST_SLOPPY_ARGUMENTS_ELEMENTS) {
    PrintFixedArrayElements(os, arguments_store);
  } else {
    DCHECK_EQ(kind, SLOW_SLOPPY_ARGUMENTS_ELEMENTS);
    PrintDictionaryElements(os, arguments_store);
  }
}

}  // namespace

void JSObject::PrintElements(std::ostream& os) {  // NOLINT
  // Don't call GetElementsKind, its validation code can cause the printer to
  // fail when debugging.
  os << " - elements = " << Brief(elements()) << " {";
  if (elements()->length() == 0) {
    os << " }\n";
    return;
  }
  switch (map()->elements_kind()) {
    case HOLEY_SMI_ELEMENTS:
    case PACKED_SMI_ELEMENTS:
    case HOLEY_ELEMENTS:
    case PACKED_ELEMENTS:
    case FAST_STRING_WRAPPER_ELEMENTS: {
      PrintFixedArrayElements(os, FixedArray::cast(elements()));
      break;
    }
    case HOLEY_DOUBLE_ELEMENTS:
    case PACKED_DOUBLE_ELEMENTS: {
      DoPrintElements<FixedDoubleArray>(os, elements());
      break;
    }

#define PRINT_ELEMENTS(Type, type, TYPE, elementType, size) \
  case TYPE##_ELEMENTS: {                                   \
    DoPrintElements<Fixed##Type##Array>(os, elements());    \
    break;                                                  \
  }
      TYPED_ARRAYS(PRINT_ELEMENTS)
#undef PRINT_ELEMENTS

    case DICTIONARY_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
      PrintDictionaryElements(os, elements());
      break;
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
      PrintSloppyArgumentElements(os, map()->elements_kind(),
                                  SloppyArgumentsElements::cast(elements()));
      break;
    case NO_ELEMENTS:
      break;
  }
  os << "\n }\n";
}


static void JSObjectPrintHeader(std::ostream& os, JSObject* obj,
                                const char* id) {  // NOLINT
  obj->PrintHeader(os, id);
  // Don't call GetElementsKind, its validation code can cause the printer to
  // fail when debugging.
  os << "\n - map = " << reinterpret_cast<void*>(obj->map()) << " [";
  if (obj->HasFastProperties()) {
    os << "FastProperties";
  } else {
    os << "DictionaryProperties";
  }
  PrototypeIterator iter(obj->GetIsolate(), obj);
  os << "]\n - prototype = " << reinterpret_cast<void*>(iter.GetCurrent());
  os << "\n - elements = " << Brief(obj->elements()) << " ["
     << ElementsKindToString(obj->map()->elements_kind());
  if (obj->elements()->IsCowArray()) os << " (COW)";
  os << "]";
  Object* hash = obj->GetHash();
  if (hash->IsSmi()) {
    os << "\n - hash = " << Brief(hash);
  }
  if (obj->GetEmbedderFieldCount() > 0) {
    os << "\n - embedder fields: " << obj->GetEmbedderFieldCount();
  }
}


static void JSObjectPrintBody(std::ostream& os, JSObject* obj,  // NOLINT
                              bool print_elements = true) {
  os << "\n - properties = ";
  Object* properties_or_hash = obj->raw_properties_or_hash();
  if (!properties_or_hash->IsSmi()) {
    os << Brief(properties_or_hash);
  }
  os << " {";
  if (obj->PrintProperties(os)) os << "\n ";
  os << "}\n";
  if (print_elements && obj->elements()->length() > 0) {
    obj->PrintElements(os);
  }
  int embedder_fields = obj->GetEmbedderFieldCount();
  if (embedder_fields > 0) {
    os << " - embedder fields = {";
    for (int i = 0; i < embedder_fields; i++) {
      os << "\n    " << obj->GetEmbedderField(i);
    }
    os << "\n }\n";
  }
}


void JSObject::JSObjectPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, nullptr);
  JSObjectPrintBody(os, this);
}

void JSArray::JSArrayPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSArray");
  os << "\n - length = " << Brief(this->length());
  JSObjectPrintBody(os, this);
}

void JSPromise::JSPromisePrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSPromise");
  os << "\n - status = " << JSPromise::Status(status());
  os << "\n - result = " << Brief(result());
  os << "\n - deferred_promise: " << Brief(deferred_promise());
  os << "\n - deferred_on_resolve: " << Brief(deferred_on_resolve());
  os << "\n - deferred_on_reject: " << Brief(deferred_on_reject());
  os << "\n - fulfill_reactions = " << Brief(fulfill_reactions());
  os << "\n - reject_reactions = " << Brief(reject_reactions());
  os << "\n - has_handler = " << has_handler();
  os << "\n ";
}

void JSRegExp::JSRegExpPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSRegExp");
  os << "\n - data = " << Brief(data());
  os << "\n - source = " << Brief(source());
  JSObjectPrintBody(os, this);
}


void Symbol::SymbolPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Symbol");
  os << "\n - hash: " << Hash();
  os << "\n - name: " << Brief(name());
  if (name()->IsUndefined(GetIsolate())) {
    os << " (" << PrivateSymbolToName() << ")";
  }
  os << "\n - private: " << is_private();
  os << "\n";
}


void Map::MapPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Map");
  os << "\n - type: " << instance_type();
  os << "\n - instance size: " << instance_size();
  if (IsJSObjectMap()) {
    os << "\n - inobject properties: " << GetInObjectProperties();
  }
  os << "\n - elements kind: " << ElementsKindToString(elements_kind());
  os << "\n - unused property fields: " << UnusedPropertyFields();
  os << "\n - enum length: ";
  if (EnumLength() == kInvalidEnumCacheSentinel) {
    os << "invalid";
  } else {
    os << EnumLength();
  }
  if (is_deprecated()) os << "\n - deprecated_map";
  if (is_stable()) os << "\n - stable_map";
  if (is_migration_target()) os << "\n - migration_target";
  if (is_dictionary_map()) os << "\n - dictionary_map";
  if (has_hidden_prototype()) os << "\n - has_hidden_prototype";
  if (has_named_interceptor()) os << "\n - named_interceptor";
  if (has_indexed_interceptor()) os << "\n - indexed_interceptor";
  if (may_have_interesting_symbols()) os << "\n - may_have_interesting_symbols";
  if (is_undetectable()) os << "\n - undetectable";
  if (is_callable()) os << "\n - callable";
  if (is_constructor()) os << "\n - constructor";
  if (has_prototype_slot()) {
    os << "\n - has_prototype_slot";
    if (has_non_instance_prototype()) os << " (non-instance prototype)";
  }
  if (is_access_check_needed()) os << "\n - access_check_needed";
  if (!is_extensible()) os << "\n - non-extensible";
  if (is_prototype_map()) {
    os << "\n - prototype_map";
    os << "\n - prototype info: " << Brief(prototype_info());
  } else {
    os << "\n - back pointer: " << Brief(GetBackPointer());
  }
  os << "\n - instance descriptors " << (owns_descriptors() ? "(own) " : "")
     << "#" << NumberOfOwnDescriptors() << ": "
     << Brief(instance_descriptors());
  if (FLAG_unbox_double_fields) {
    os << "\n - layout descriptor: ";
    layout_descriptor()->ShortPrint(os);
  }
  {
    DisallowHeapAllocation no_gc;
    TransitionsAccessor transitions(this, &no_gc);
    int nof_transitions = transitions.NumberOfTransitions();
    if (nof_transitions > 0) {
      os << "\n - transitions #" << nof_transitions << ": "
         << Brief(raw_transitions());
      transitions.PrintTransitions(os);
    }
  }
  os << "\n - prototype: " << Brief(prototype());
  os << "\n - constructor: " << Brief(GetConstructor());
  os << "\n - dependent code: " << Brief(dependent_code());
  os << "\n - construction counter: " << construction_counter();
  os << "\n";
}


void AliasedArgumentsEntry::AliasedArgumentsEntryPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AliasedArgumentsEntry");
  os << "\n - aliased_context_slot: " << aliased_context_slot();
}


void FixedArray::FixedArrayPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, IsHashTable() ? "HashTable" : "FixedArray");
  os << "\n - map = " << Brief(map());
  os << "\n - length: " << length();
  PrintFixedArrayElements(os, this);
  os << "\n";
}

void PropertyArray::PropertyArrayPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PropertyArray");
  os << "\n - map = " << Brief(map());
  os << "\n - length: " << length();
  os << "\n - hash: " << Hash();
  PrintFixedArrayElements(os, this);
  os << "\n";
}

void FixedDoubleArray::FixedDoubleArrayPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "FixedDoubleArray");
  os << "\n - map = " << Brief(map());
  os << "\n - length: " << length();
  DoPrintElements<FixedDoubleArray>(os, this);
  os << "\n";
}


void TransitionArray::TransitionArrayPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "TransitionArray");
  os << "\n - capacity: " << length();
  for (int i = 0; i < length(); i++) {
    os << "\n  [" << i << "]: " << Brief(get(i));
    if (i == kPrototypeTransitionsIndex) os << " (prototype transitions)";
    if (i == kTransitionLengthIndex) os << " (number of transitions)";
  }
  os << "\n";
}

template void FeedbackVectorSpecBase<StaticFeedbackVectorSpec>::Print();
template void FeedbackVectorSpecBase<FeedbackVectorSpec>::Print();

template <typename Derived>
void FeedbackVectorSpecBase<Derived>::Print() {
  OFStream os(stdout);
  FeedbackVectorSpecPrint(os);
  os << std::flush;
}

template <typename Derived>
void FeedbackVectorSpecBase<Derived>::FeedbackVectorSpecPrint(
    std::ostream& os) {  // NOLINT
  int slot_count = This()->slots();
  os << " - slot_count: " << slot_count;
  if (slot_count == 0) {
    os << " (empty)\n";
    return;
  }

  for (int slot = 0; slot < slot_count;) {
    FeedbackSlotKind kind = This()->GetKind(FeedbackSlot(slot));
    int entry_size = FeedbackMetadata::GetSlotSize(kind);
    DCHECK_LT(0, entry_size);
    os << "\n Slot #" << slot << " " << kind;
    slot += entry_size;
  }
  os << "\n";
}

void FeedbackMetadata::Print() {
  OFStream os(stdout);
  FeedbackMetadataPrint(os);
  os << std::flush;
}

void FeedbackMetadata::FeedbackMetadataPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "FeedbackMetadata");
  os << "\n - length: " << length();
  if (length() == 0) {
    os << " (empty)\n";
    return;
  }
  os << "\n - slot_count: " << slot_count();

  FeedbackMetadataIterator iter(this);
  while (iter.HasNext()) {
    FeedbackSlot slot = iter.Next();
    FeedbackSlotKind kind = iter.kind();
    os << "\n Slot " << slot << " " << kind;
  }
  os << "\n";
}

void FeedbackVector::Print() {
  OFStream os(stdout);
  FeedbackVectorPrint(os);
  os << std::flush;
}

void FeedbackVector::FeedbackVectorPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "FeedbackVector");
  os << "\n - length: " << length();
  if (length() == 0) {
    os << " (empty)\n";
    return;
  }

  os << "\n - shared function info: " << Brief(shared_function_info());
  os << "\n - optimized code/marker: ";
  if (has_optimized_code()) {
    os << Brief(optimized_code());
  } else {
    os << optimization_marker();
  }
  os << "\n - invocation count: " << invocation_count();
  os << "\n - profiler ticks: " << profiler_ticks();

  FeedbackMetadataIterator iter(metadata());
  while (iter.HasNext()) {
    FeedbackSlot slot = iter.Next();
    FeedbackSlotKind kind = iter.kind();

    os << "\n - slot " << slot << " " << kind << " ";
    FeedbackSlotPrint(os, slot, kind);

    int entry_size = iter.entry_size();
    if (entry_size > 0) os << " {";
    for (int i = 0; i < entry_size; i++) {
      int index = GetIndex(slot) + i;
      os << "\n     [" << index << "]: " << Brief(get(index));
    }
    if (entry_size > 0) os << "\n  }";
  }
  os << "\n";
}

void FeedbackVector::FeedbackSlotPrint(std::ostream& os,
                                       FeedbackSlot slot) {  // NOLINT
  FeedbackSlotPrint(os, slot, GetKind(slot));
}

void FeedbackVector::FeedbackSlotPrint(std::ostream& os, FeedbackSlot slot,
                                       FeedbackSlotKind kind) {  // NOLINT
  switch (kind) {
    case FeedbackSlotKind::kLoadProperty: {
      LoadICNexus nexus(this, slot);
      os << Code::ICState2String(nexus.StateFromFeedback());
      break;
    }
    case FeedbackSlotKind::kLoadGlobalInsideTypeof:
    case FeedbackSlotKind::kLoadGlobalNotInsideTypeof: {
      LoadGlobalICNexus nexus(this, slot);
      os << Code::ICState2String(nexus.StateFromFeedback());
      break;
    }
    case FeedbackSlotKind::kLoadKeyed: {
      KeyedLoadICNexus nexus(this, slot);
      os << Code::ICState2String(nexus.StateFromFeedback());
      break;
    }
    case FeedbackSlotKind::kCall: {
      CallICNexus nexus(this, slot);
      os << Code::ICState2String(nexus.StateFromFeedback());
      break;
    }
    case FeedbackSlotKind::kStoreNamedSloppy:
    case FeedbackSlotKind::kStoreNamedStrict:
    case FeedbackSlotKind::kStoreOwnNamed:
    case FeedbackSlotKind::kStoreGlobalSloppy:
    case FeedbackSlotKind::kStoreGlobalStrict: {
      StoreICNexus nexus(this, slot);
      os << Code::ICState2String(nexus.StateFromFeedback());
      break;
    }
    case FeedbackSlotKind::kStoreKeyedSloppy:
    case FeedbackSlotKind::kStoreKeyedStrict: {
      KeyedStoreICNexus nexus(this, slot);
      os << Code::ICState2String(nexus.StateFromFeedback());
      break;
    }
    case FeedbackSlotKind::kBinaryOp: {
      BinaryOpICNexus nexus(this, slot);
      os << "BinaryOp:" << nexus.GetBinaryOperationFeedback();
      break;
    }
    case FeedbackSlotKind::kCompareOp: {
      CompareICNexus nexus(this, slot);
      os << "CompareOp:" << nexus.GetCompareOperationFeedback();
      break;
    }
    case FeedbackSlotKind::kForIn: {
      ForInICNexus nexus(this, slot);
      os << "ForIn:" << nexus.GetForInFeedback();
      break;
    }
    case FeedbackSlotKind::kInstanceOf: {
      InstanceOfICNexus nexus(this, slot);
      os << Code::ICState2String(nexus.StateFromFeedback());
      break;
    }
    case FeedbackSlotKind::kStoreDataPropertyInLiteral: {
      StoreDataPropertyInLiteralICNexus nexus(this, slot);
      os << Code::ICState2String(nexus.StateFromFeedback());
      break;
    }
    case FeedbackSlotKind::kCreateClosure:
    case FeedbackSlotKind::kLiteral:
    case FeedbackSlotKind::kTypeProfile:
      break;
    case FeedbackSlotKind::kInvalid:
    case FeedbackSlotKind::kKindsNumber:
      UNREACHABLE();
      break;
  }
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
  if (!HasOnlyOneByteChars()) {
    os << "u";
  }
  if (StringShape(this).IsInternalized()) {
    os << "#";
  } else if (StringShape(this).IsCons()) {
    os << "c\"";
  } else if (StringShape(this).IsThin()) {
    os << ">\"";
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
    SNPrintF(buf, "\n - time = %s %04d/%02d/%02d %02d:%02d:%02d\n",
             weekdays[weekday()->IsSmi() ? Smi::ToInt(weekday()) + 1 : 0],
             year()->IsSmi() ? Smi::ToInt(year()) : -1,
             month()->IsSmi() ? Smi::ToInt(month()) : -1,
             day()->IsSmi() ? Smi::ToInt(day()) : -1,
             hour()->IsSmi() ? Smi::ToInt(hour()) : -1,
             min()->IsSmi() ? Smi::ToInt(min()) : -1,
             sec()->IsSmi() ? Smi::ToInt(sec()) : -1);
    os << buf.start();
  }
  JSObjectPrintBody(os, this);
}


void JSProxy::JSProxyPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSProxy");
  os << "\n - map = " << reinterpret_cast<void*>(map());
  os << "\n - target = ";
  target()->ShortPrint(os);
  os << "\n - handler = ";
  handler()->ShortPrint(os);
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

void JSCollectionIterator::JSCollectionIteratorPrint(
    std::ostream& os) {  // NOLINT
  os << "\n - table = " << Brief(table());
  os << "\n - index = " << Brief(index());
  os << "\n";
}


void JSSetIterator::JSSetIteratorPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSSetIterator");
  JSCollectionIteratorPrint(os);
}


void JSMapIterator::JSMapIteratorPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSMapIterator");
  JSCollectionIteratorPrint(os);
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
  if (is_external()) os << "\n - external";
  if (is_neuterable()) os << "\n - neuterable";
  if (was_neutered()) os << "\n - neutered";
  if (is_shared()) os << "\n - shared";
  if (has_guard_region()) os << "\n - has_guard_region";
  if (is_growable()) os << "\n - growable";
  JSObjectPrintBody(os, this, !was_neutered());
}


void JSTypedArray::JSTypedArrayPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSTypedArray");
  os << "\n - buffer = " << Brief(buffer());
  os << "\n - byte_offset = " << Brief(byte_offset());
  os << "\n - byte_length = " << Brief(byte_length());
  os << "\n - length = " << Brief(length());
  if (WasNeutered()) os << "\n - neutered";
  JSObjectPrintBody(os, this, !WasNeutered());
}

void JSArrayIterator::JSArrayIteratorPrint(std::ostream& os) {  // NOLING
  JSObjectPrintHeader(os, this, "JSArrayIterator");

  InstanceType instance_type = map()->instance_type();
  std::string type;
  if (instance_type <= LAST_ARRAY_KEY_ITERATOR_TYPE) {
    type = "keys";
  } else if (instance_type <= LAST_ARRAY_KEY_VALUE_ITERATOR_TYPE) {
    type = "entries";
  } else {
    type = "values";
  }

  os << "\n - type = " << type;
  os << "\n - object = " << Brief(object());
  os << "\n - index = " << Brief(index());

  JSObjectPrintBody(os, this);
}

void JSDataView::JSDataViewPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSDataView");
  os << "\n - buffer =" << Brief(buffer());
  os << "\n - byte_offset = " << Brief(byte_offset());
  os << "\n - byte_length = " << Brief(byte_length());
  if (WasNeutered()) os << "\n - neutered";
  JSObjectPrintBody(os, this, !WasNeutered());
}


void JSBoundFunction::JSBoundFunctionPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSBoundFunction");
  os << "\n - bound_target_function = " << Brief(bound_target_function());
  os << "\n - bound_this = " << Brief(bound_this());
  os << "\n - bound_arguments = " << Brief(bound_arguments());
  JSObjectPrintBody(os, this);
}


namespace {

std::ostream& operator<<(std::ostream& os, FunctionKind kind) {
  os << "[";
  if (kind == FunctionKind::kNormalFunction) {
    os << " NormalFunction";
  } else {
#define PRINT_FLAG(name)                                                  \
  if (static_cast<int>(kind) & static_cast<int>(FunctionKind::k##name)) { \
    os << " " << #name;                                                   \
  }

    PRINT_FLAG(ArrowFunction)
    PRINT_FLAG(GeneratorFunction)
    PRINT_FLAG(ConciseMethod)
    PRINT_FLAG(DefaultConstructor)
    PRINT_FLAG(DerivedConstructor)
    PRINT_FLAG(BaseConstructor)
    PRINT_FLAG(GetterFunction)
    PRINT_FLAG(SetterFunction)
    PRINT_FLAG(AsyncFunction)
    PRINT_FLAG(Module)
#undef PRINT_FLAG
  }
  return os << " ]";
}

}  // namespace

void JSFunction::JSFunctionPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "Function");
  os << "\n - function prototype = ";
  if (has_prototype_slot()) {
    if (has_prototype()) {
      os << Brief(prototype());
      if (map()->has_non_instance_prototype()) {
        os << " (non-instance prototype)";
      }
    }
    os << "\n - initial_map = ";
    if (has_initial_map()) os << Brief(initial_map());
  } else {
    os << "<no-prototype-slot>";
  }
  os << "\n - shared_info = " << Brief(shared());
  os << "\n - name = " << Brief(shared()->name());
  os << "\n - formal_parameter_count = "
     << shared()->internal_formal_parameter_count();
  os << "\n - kind = " << shared()->kind();
  os << "\n - context = " << Brief(context());
  os << "\n - code = " << Brief(code());
  if (IsInterpreted()) {
    os << "\n - interpreted";
    if (shared()->HasBytecodeArray()) {
      os << "\n - bytecode = " << shared()->bytecode_array();
    }
  }
  if (WasmExportedFunction::IsWasmExportedFunction(this)) {
    WasmExportedFunction* function = WasmExportedFunction::cast(this);
    os << "\n - WASM instance "
       << reinterpret_cast<void*>(function->instance());
    os << "\n   context "
       << reinterpret_cast<void*>(function->instance()->wasm_context()->get());
    os << "\n - WASM function index " << function->function_index();
  }
  shared()->PrintSourceCode(os);
  JSObjectPrintBody(os, this);
  os << "\n - feedback vector: ";
  if (feedback_vector_cell()->value()->IsFeedbackVector()) {
    feedback_vector()->FeedbackVectorPrint(os);
  } else {
    os << "not available\n";
  }
}

void SharedFunctionInfo::PrintSourceCode(std::ostream& os) {
  if (HasSourceCode()) {
    os << "\n - source code = ";
    String* source = String::cast(Script::cast(script())->source());
    int start = start_position();
    int length = end_position() - start;
    std::unique_ptr<char[]> source_string = source->ToCString(
        DISALLOW_NULLS, FAST_STRING_TRAVERSAL, start, length, nullptr);
    os << source_string.get();
  }
}

void SharedFunctionInfo::SharedFunctionInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "SharedFunctionInfo");
  os << "\n - name = ";
  if (has_shared_name()) {
    os << Brief(raw_name());
  } else {
    os << "<no-shared-name>";
  }
  os << "\n - kind = " << kind();
  if (needs_home_object()) {
    os << "\n - needs_home_object";
  }
  os << "\n - function_map_index = " << function_map_index();
  os << "\n - formal_parameter_count = " << internal_formal_parameter_count();
  os << "\n - expected_nof_properties = " << expected_nof_properties();
  os << "\n - language_mode = " << language_mode();
  os << "\n - instance class name = ";
  instance_class_name()->Print(os);
  os << " - code = " << Brief(code());
  if (HasBytecodeArray()) {
    os << "\n - bytecode_array = " << bytecode_array();
  }
  if (HasAsmWasmData()) {
    os << "\n - asm_wasm_data = " << Brief(asm_wasm_data());
  }
  PrintSourceCode(os);
  // Script files are often large, hard to read.
  // os << "\n - script =";
  // script()->Print(os);
  if (is_named_expression()) {
    os << "\n - named expression";
  } else if (is_anonymous_expression()) {
    os << "\n - anonymous expression";
  } else if (is_declaration()) {
    os << "\n - declaration";
  }
  os << "\n - function token position = " << function_token_position();
  os << "\n - start position = " << start_position();
  os << "\n - end position = " << end_position();
  if (HasDebugInfo()) {
    os << "\n - debug info = " << Brief(debug_info());
  } else {
    os << "\n - no debug info";
  }
  os << "\n - length = " << length();
  os << "\n - feedback_metadata = ";
  feedback_metadata()->FeedbackMetadataPrint(os);
  if (HasPreParsedScopeData()) {
    os << "\n - preparsed scope data = " << preparsed_scope_data();
  } else {
    os << "\n - no preparsed scope data";
  }
  os << "\n";
}


void JSGlobalProxy::JSGlobalProxyPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSGlobalProxy");
  if (!GetIsolate()->bootstrapper()->IsActive()) {
    os << "\n - native context = " << Brief(native_context());
  }
  JSObjectPrintBody(os, this);
}


void JSGlobalObject::JSGlobalObjectPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSGlobalObject");
  if (!GetIsolate()->bootstrapper()->IsActive()) {
    os << "\n - native context = " << Brief(native_context());
  }
  os << "\n - global proxy = " << Brief(global_proxy());
  JSObjectPrintBody(os, this);
}


void Cell::CellPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Cell");
  os << "\n - value: " << Brief(value());
  os << "\n";
}


void PropertyCell::PropertyCellPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PropertyCell");
  os << "\n - name: ";
  name()->NamePrint(os);
  os << "\n - value: " << Brief(value());
  os << "\n - details: ";
  property_details().PrintAsSlowTo(os);
  PropertyCellType cell_type = property_details().cell_type();
  os << "\n - cell_type: ";
  if (value()->IsTheHole(GetIsolate())) {
    switch (cell_type) {
      case PropertyCellType::kUninitialized:
        os << "Uninitialized";
        break;
      case PropertyCellType::kInvalidated:
        os << "Invalidated";
        break;
      default:
        os << "??? " << static_cast<int>(cell_type);
        break;
    }
  } else {
    switch (cell_type) {
      case PropertyCellType::kUndefined:
        os << "Undefined";
        break;
      case PropertyCellType::kConstant:
        os << "Constant";
        break;
      case PropertyCellType::kConstantType:
        os << "ConstantType"
           << " (";
        switch (GetConstantType()) {
          case PropertyCellConstantType::kSmi:
            os << "Smi";
            break;
          case PropertyCellConstantType::kStableMap:
            os << "StableMap";
            break;
        }
        os << ")";
        break;
      case PropertyCellType::kMutable:
        os << "Mutable";
        break;
    }
  }
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
  os << "\n";
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_use_verbose_printer) {
    Disassemble(nullptr, os);
  }
#endif
}

void CodeDataContainer::CodeDataContainerPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "CodeDataContainer");
  os << "\n - kind_specific_flags: " << kind_specific_flags();
  os << "\n";
}

void Foreign::ForeignPrint(std::ostream& os) {  // NOLINT
  os << "foreign address : " << reinterpret_cast<void*>(foreign_address());
  os << "\n";
}


void AccessorInfo::AccessorInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AccessorInfo");
  os << "\n - name: " << Brief(name());
  os << "\n - flags: " << flags();
  os << "\n - getter: " << Brief(getter());
  os << "\n - setter: " << Brief(setter());
  os << "\n - js_getter: " << Brief(js_getter());
  os << "\n - data: " << Brief(data());
  os << "\n";
}

void PromiseCapability::PromiseCapabilityPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PromiseCapability");
  os << "\n - promise: " << Brief(promise());
  os << "\n - resolve: " << Brief(resolve());
  os << "\n - reject: " << Brief(reject());
  os << "\n";
}

void PromiseResolveThenableJobInfo::PromiseResolveThenableJobInfoPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PromiseResolveThenableJobInfo");
  os << "\n - thenable: " << Brief(thenable());
  os << "\n - then: " << Brief(then());
  os << "\n - resolve: " << Brief(resolve());
  os << "\n - reject: " << Brief(reject());
  os << "\n - context: " << Brief(context());
  os << "\n";
}

void PromiseReactionJobInfo::PromiseReactionJobInfoPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PromiseReactionJobInfo");
  os << "\n - value: " << Brief(value());
  os << "\n - tasks: " << Brief(tasks());
  os << "\n - deferred_promise: " << Brief(deferred_promise());
  os << "\n - deferred_on_resolve: " << Brief(deferred_on_resolve());
  os << "\n - deferred_on_reject: " << Brief(deferred_on_reject());
  os << "\n - reaction context: " << Brief(context());
  os << "\n";
}

void AsyncGeneratorRequest::AsyncGeneratorRequestPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AsyncGeneratorRequest");
  const char* mode = "Invalid!";
  switch (resume_mode()) {
    case JSGeneratorObject::kNext:
      mode = ".next()";
      break;
    case JSGeneratorObject::kReturn:
      mode = ".return()";
      break;
    case JSGeneratorObject::kThrow:
      mode = ".throw()";
      break;
  }
  os << "\n - resume mode: " << mode;
  os << "\n - value: " << Brief(value());
  os << "\n - next: " << Brief(next());
  os << "\n";
}

void ModuleInfoEntry::ModuleInfoEntryPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "ModuleInfoEntry");
  os << "\n - export_name: " << Brief(export_name());
  os << "\n - local_name: " << Brief(local_name());
  os << "\n - import_name: " << Brief(import_name());
  os << "\n - module_request: " << module_request();
  os << "\n - cell_index: " << cell_index();
  os << "\n - beg_pos: " << beg_pos();
  os << "\n - end_pos: " << end_pos();
  os << "\n";
}

void Module::ModulePrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Module");
  os << "\n - origin: " << Brief(script()->GetNameOrSourceURL());
  os << "\n - code: " << Brief(code());
  os << "\n - exports: " << Brief(exports());
  os << "\n - requested_modules: " << Brief(requested_modules());
  os << "\n - script: " << Brief(script());
  os << "\n - import_meta: " << Brief(import_meta());
  os << "\n - status: " << status();
  os << "\n - exception: " << Brief(exception());
  os << "\n";
}

void JSModuleNamespace::JSModuleNamespacePrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSModuleNamespace");
  os << "\n - module: " << Brief(module());
  JSObjectPrintBody(os, this);
}

void PrototypeInfo::PrototypeInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PrototypeInfo");
  os << "\n - weak cell: " << Brief(weak_cell());
  os << "\n - prototype users: " << Brief(prototype_users());
  os << "\n - registry slot: " << registry_slot();
  os << "\n - validity cell: " << Brief(validity_cell());
  os << "\n - object create map: " << Brief(object_create_map());
  os << "\n - should_be_fast_map: " << should_be_fast_map();
  os << "\n";
}

void Tuple2::Tuple2Print(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Tuple2");
  os << "\n - value1: " << Brief(value1());
  os << "\n - value2: " << Brief(value2());
  os << "\n";
}

void Tuple3::Tuple3Print(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Tuple3");
  os << "\n - value1: " << Brief(value1());
  os << "\n - value2: " << Brief(value2());
  os << "\n - value3: " << Brief(value3());
  os << "\n";
}

void LoadHandler::LoadHandlerPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "LoadHandler");
  // TODO(ishell): implement printing based on handler kind
  os << "\n - handler: " << Brief(smi_handler());
  os << "\n - validity_cell: " << Brief(validity_cell());
  int data_count = data_field_count();
  if (data_count >= 1) {
    os << "\n - data1: " << Brief(data1());
  }
  if (data_count >= 2) {
    os << "\n - data2: " << Brief(data2());
  }
  if (data_count >= 3) {
    os << "\n - data3: " << Brief(data3());
  }
  os << "\n";
}

void StoreHandler::StoreHandlerPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "StoreHandler");
  // TODO(ishell): implement printing based on handler kind
  os << "\n - handler: " << Brief(smi_handler());
  os << "\n - validity_cell: " << Brief(validity_cell());
  int data_count = data_field_count();
  if (data_count >= 1) {
    os << "\n - data1: " << Brief(data1());
  }
  if (data_count >= 2) {
    os << "\n - data2: " << Brief(data2());
  }
  if (data_count >= 3) {
    os << "\n - data3: " << Brief(data3());
  }
  os << "\n";
}

void ContextExtension::ContextExtensionPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "ContextExtension");
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
  os << "\n - callback: " << Brief(callback());
  os << "\n - named_interceptor: " << Brief(named_interceptor());
  os << "\n - indexed_interceptor: " << Brief(indexed_interceptor());
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


void FunctionTemplateInfo::FunctionTemplateInfoPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "FunctionTemplateInfo");
  os << "\n - class name: " << Brief(class_name());
  os << "\n - tag: " << Brief(tag());
  os << "\n - serial_number: " << Brief(serial_number());
  os << "\n - property_list: " << Brief(property_list());
  os << "\n - call_code: " << Brief(call_code());
  os << "\n - property_accessors: " << Brief(property_accessors());
  os << "\n - prototype_template: " << Brief(prototype_template());
  os << "\n - parent_template: " << Brief(parent_template());
  os << "\n - named_property_handler: " << Brief(named_property_handler());
  os << "\n - indexed_property_handler: " << Brief(indexed_property_handler());
  os << "\n - instance_template: " << Brief(instance_template());
  os << "\n - signature: " << Brief(signature());
  os << "\n - access_check_info: " << Brief(access_check_info());
  os << "\n - cached_property_name: " << Brief(cached_property_name());
  os << "\n - hidden_prototype: " << (hidden_prototype() ? "true" : "false");
  os << "\n - undetectable: " << (undetectable() ? "true" : "false");
  os << "\n - need_access_check: " << (needs_access_check() ? "true" : "false");
  os << "\n - instantiated: " << (instantiated() ? "true" : "false");
  os << "\n";
}


void ObjectTemplateInfo::ObjectTemplateInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "ObjectTemplateInfo");
  os << "\n - tag: " << Brief(tag());
  os << "\n - serial_number: " << Brief(serial_number());
  os << "\n - property_list: " << Brief(property_list());
  os << "\n - property_accessors: " << Brief(property_accessors());
  os << "\n - constructor: " << Brief(constructor());
  os << "\n - embedder_field_count: " << embedder_field_count();
  os << "\n - immutable_proto: " << (immutable_proto() ? "true" : "false");
  os << "\n";
}


void AllocationSite::AllocationSitePrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AllocationSite");
  os << "\n - weak_next: " << Brief(weak_next());
  os << "\n - dependent code: " << Brief(dependent_code());
  os << "\n - nested site: " << Brief(nested_site());
  os << "\n - memento found count: "
     << Brief(Smi::FromInt(memento_found_count()));
  os << "\n - memento create count: "
     << Brief(Smi::FromInt(memento_create_count()));
  os << "\n - pretenure decision: "
     << Brief(Smi::FromInt(pretenure_decision()));
  os << "\n - transition_info: ";
  if (!PointsToLiteral()) {
    ElementsKind kind = GetElementsKind();
    os << "Array allocation with ElementsKind " << ElementsKindToString(kind);
  } else if (boilerplate()->IsJSArray()) {
    os << "Array literal with boilerplate " << Brief(boilerplate());
  } else {
    os << "Object literal with boilerplate " << Brief(boilerplate());
  }
  os << "\n";
}


void AllocationMemento::AllocationMementoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AllocationMemento");
  os << "\n - allocation site: ";
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
  if (has_eval_from_shared()) {
    os << "\n - eval from shared: " << Brief(eval_from_shared());
  }
  if (is_wrapped()) {
    os << "\n - wrapped arguments: " << Brief(wrapped_arguments());
  }
  os << "\n - eval from position: " << eval_from_position();
  os << "\n - shared function infos: " << Brief(shared_function_infos());
  os << "\n";
}


void DebugInfo::DebugInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "DebugInfo");
  os << "\n - flags: " << flags();
  os << "\n - debugger_hints: " << debugger_hints();
  os << "\n - shared: " << Brief(shared());
  os << "\n - debug bytecode array: " << Brief(debug_bytecode_array());
  os << "\n - break_points: ";
  break_points()->Print(os);
  os << "\n - coverage_info: " << Brief(coverage_info());
}


void StackFrameInfo::StackFrameInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "StackFrame");
  os << "\n - line_number: " << line_number();
  os << "\n - column_number: " << column_number();
  os << "\n - script_id: " << script_id();
  os << "\n - script_name: " << Brief(script_name());
  os << "\n - script_name_or_source_url: "
     << Brief(script_name_or_source_url());
  os << "\n - function_name: " << Brief(function_name());
  os << "\n - is_eval: " << (is_eval() ? "true" : "false");
  os << "\n - is_constructor: " << (is_constructor() ? "true" : "false");
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

void LayoutDescriptor::ShortPrint(std::ostream& os) {
  if (IsSmi()) {
    os << this;  // Print tagged value for easy use with "jld" gdb macro.
  } else {
    os << Brief(this);
  }
}

void LayoutDescriptor::Print(std::ostream& os) {  // NOLINT
  os << "Layout descriptor: ";
  if (IsFastPointerLayout()) {
    os << "<all tagged>";
  } else if (IsSmi()) {
    os << "fast";
    PrintBitMask(os, static_cast<uint32_t>(Smi::ToInt(this)));
  } else if (IsOddball() &&
             IsUninitialized(HeapObject::cast(this)->GetIsolate())) {
    os << "<uninitialized>";
  } else {
    os << "slow";
    int num_words = number_of_layout_words();
    for (int i = 0; i < num_words; i++) {
      if (i > 0) os << " |";
      PrintBitMask(os, get_layout_word(i));
    }
  }
  os << "\n";
}

void PreParsedScopeData::PreParsedScopeDataPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PreParsedScopeData");
  os << "\n - scope_data: " << Brief(scope_data());
  os << "\n - child_data: " << Brief(child_data());
  os << "\n";
}

#endif  // OBJECT_PRINT

// TODO(cbruni): remove once the new maptracer is in place.
void Name::NameShortPrint() {
  if (this->IsString()) {
    PrintF("%s", String::cast(this)->ToCString().get());
  } else {
    DCHECK(this->IsSymbol());
    Symbol* s = Symbol::cast(this);
    if (s->name()->IsUndefined(GetIsolate())) {
      PrintF("#<%s>", s->PrivateSymbolToName());
    } else {
      PrintF("<%s>", String::cast(s->name())->ToCString().get());
    }
  }
}

// TODO(cbruni): remove once the new maptracer is in place.
int Name::NameShortPrint(Vector<char> str) {
  if (this->IsString()) {
    return SNPrintF(str, "%s", String::cast(this)->ToCString().get());
  } else {
    DCHECK(this->IsSymbol());
    Symbol* s = Symbol::cast(this);
    if (s->name()->IsUndefined(GetIsolate())) {
      return SNPrintF(str, "#<%s>", s->PrivateSymbolToName());
    } else {
      return SNPrintF(str, "<%s>", String::cast(s->name())->ToCString().get());
    }
  }
}

void Map::PrintMapDetails(std::ostream& os, JSObject* holder) {
  DisallowHeapAllocation no_gc;
#ifdef OBJECT_PRINT
  this->MapPrint(os);
#else
  os << "Map=" << reinterpret_cast<void*>(this);
#endif
  os << "\n";
  instance_descriptors()->PrintDescriptors(os);
  if (is_dictionary_map() && holder != nullptr) {
    os << holder->property_dictionary() << "\n";
  }
}

void DescriptorArray::PrintDescriptors(std::ostream& os) {  // NOLINT
  HandleScope scope(GetIsolate());
  os << "Descriptor array #" << number_of_descriptors() << ":";
  for (int i = 0; i < number_of_descriptors(); i++) {
    Name* key = GetKey(i);
    os << "\n  [" << i << "]: ";
#ifdef OBJECT_PRINT
    key->NamePrint(os);
#else
    key->ShortPrint(os);
#endif
    os << " ";
    PrintDescriptorDetails(os, i, PropertyDetails::kPrintFull);
  }
  os << "\n";
}

void DescriptorArray::PrintDescriptorDetails(std::ostream& os, int descriptor,
                                             PropertyDetails::PrintMode mode) {
  PropertyDetails details = GetDetails(descriptor);
  details.PrintAsFastTo(os, mode);
  os << " @ ";
  Object* value = GetValue(descriptor);
  switch (details.location()) {
    case kField: {
      FieldType* field_type = Map::UnwrapFieldType(value);
      field_type->PrintTo(os);
      break;
    }
    case kDescriptor:
      os << Brief(value);
      if (value->IsAccessorPair()) {
        AccessorPair* pair = AccessorPair::cast(value);
        os << "(get: " << Brief(pair->getter())
           << ", set: " << Brief(pair->setter()) << ")";
      }
      break;
  }
}

#if defined(DEBUG) || defined(OBJECT_PRINT)
// This method is only meant to be called from gdb for debugging purposes.
// Since the string can also be in two-byte encoding, non-Latin1 characters
// will be ignored in the output.
char* String::ToAsciiArray() {
  // Static so that subsequent calls frees previously allocated space.
  // This also means that previous results will be overwritten.
  static char* buffer = nullptr;
  if (buffer != nullptr) delete[] buffer;
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
// static
void TransitionsAccessor::PrintOneTransition(std::ostream& os, Name* key,
                                             Map* target, Object* raw_target) {
  os << "\n     ";
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
  } else {
    DCHECK(!IsSpecialTransition(key));
    os << "(transition to ";
    int descriptor = target->LastAdded();
    DescriptorArray* descriptors = target->instance_descriptors();
    descriptors->PrintDescriptorDetails(os, descriptor,
                                        PropertyDetails::kForTransitions);
    os << ")";
  }
  os << " -> " << Brief(target);
  if (!raw_target->IsMap() && !raw_target->IsWeakCell()) {
    os << " (handler: " << Brief(raw_target) << ")";
  }
}

void TransitionArray::Print() {
  OFStream os(stdout);
  Print(os);
}

// TODO(ishell): unify with TransitionArrayPrint().
void TransitionArray::Print(std::ostream& os) {
  int num_transitions = number_of_transitions();
  os << "Transition array #" << num_transitions << ":";
  for (int i = 0; i < num_transitions; i++) {
    Name* key = GetKey(i);
    Map* target = GetTarget(i);
    Object* raw_target = GetRawTarget(i);
    TransitionsAccessor::PrintOneTransition(os, key, target, raw_target);
  }
  os << "\n" << std::flush;
}

void TransitionsAccessor::PrintTransitions(std::ostream& os) {  // NOLINT
  WeakCell* cell = nullptr;
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
      return;
    case kWeakCell:
      cell = GetTargetCell<kWeakCell>();
      break;
    case kHandler:
      cell = GetTargetCell<kHandler>();
      break;
    case kFullTransitionArray:
      return transitions()->Print(os);
  }
  DCHECK(!cell->cleared());
  Map* target = Map::cast(cell->value());
  Name* key = GetSimpleTransitionKey(target);
  PrintOneTransition(os, key, target, raw_transitions_);
}

void TransitionsAccessor::PrintTransitionTree() {
  OFStream os(stdout);
  os << "map= " << Brief(map_);
  DisallowHeapAllocation no_gc;
  PrintTransitionTree(os, 0, &no_gc);
  os << "\n" << std::flush;
}

void TransitionsAccessor::PrintTransitionTree(std::ostream& os, int level,
                                              DisallowHeapAllocation* no_gc) {
  int num_transitions = NumberOfTransitions();
  if (num_transitions == 0) return;
  for (int i = 0; i < num_transitions; i++) {
    Name* key = GetKey(i);
    Map* target = GetTarget(i);
    os << std::endl
       << "  " << level << "/" << i << ":" << std::setw(level * 2 + 2) << " ";
    std::stringstream ss;
    ss << Brief(target);
    os << std::left << std::setw(50) << ss.str() << ": ";

    Heap* heap = key->GetHeap();
    if (key == heap->nonextensible_symbol()) {
      os << "to non-extensible";
    } else if (key == heap->sealed_symbol()) {
      os << "to sealed ";
    } else if (key == heap->frozen_symbol()) {
      os << "to frozen";
    } else if (key == heap->elements_transition_symbol()) {
      os << "to " << ElementsKindToString(target->elements_kind());
    } else if (key == heap->strict_function_transition_symbol()) {
      os << "to strict function";
    } else {
#ifdef OBJECT_PRINT
      key->NamePrint(os);
#else
      key->ShortPrint(os);
#endif
      os << " ";
      DCHECK(!IsSpecialTransition(key));
      os << "to ";
      int descriptor = target->LastAdded();
      DescriptorArray* descriptors = target->instance_descriptors();
      descriptors->PrintDescriptorDetails(os, descriptor,
                                          PropertyDetails::kForTransitions);
    }
    TransitionsAccessor transitions(target, no_gc);
    transitions.PrintTransitionTree(os, level + 1, no_gc);
  }
}

void JSObject::PrintTransitions(std::ostream& os) {  // NOLINT
  DisallowHeapAllocation no_gc;
  TransitionsAccessor ta(map(), &no_gc);
  if (ta.NumberOfTransitions() == 0) return;
  os << "\n - transitions";
  ta.PrintTransitions(os);
}
#endif  // defined(DEBUG) || defined(OBJECT_PRINT)
}  // namespace internal
}  // namespace v8

//
// The following functions are used by our gdb macros.
//
extern void _v8_internal_Print_Object(void* object) {
  reinterpret_cast<i::Object*>(object)->Print();
}

extern void _v8_internal_Print_Code(void* object) {
  i::Address address = reinterpret_cast<i::Address>(object);
  i::Isolate* isolate = i::Isolate::Current();

  i::wasm::WasmCode* wasm_code =
      isolate->wasm_engine()->code_manager()->LookupCode(address);
  if (wasm_code) {
    wasm_code->Print(isolate);
    return;
  }

  if (!isolate->heap()->InSpaceSlow(address, i::CODE_SPACE) &&
      !isolate->heap()->InSpaceSlow(address, i::LO_SPACE)) {
    i::PrintF(
        "%p is not within the current isolate's large object or code spaces\n",
        static_cast<void*>(address));
    return;
  }

  i::Code* code = isolate->FindCodeObject(address);
  if (!code->IsCode()) {
    i::PrintF("No code object found containing %p\n",
              static_cast<void*>(address));
    return;
  }
#ifdef ENABLE_DISASSEMBLER
  i::OFStream os(stdout);
  code->Disassemble(nullptr, os, address);
#else   // ENABLE_DISASSEMBLER
  code->Print();
#endif  // ENABLE_DISASSEMBLER
}

extern void _v8_internal_Print_FeedbackMetadata(void* object) {
  if (reinterpret_cast<i::Object*>(object)->IsSmi()) {
    printf("Please provide a feedback metadata object\n");
  } else {
    reinterpret_cast<i::FeedbackMetadata*>(object)->Print();
  }
}

extern void _v8_internal_Print_FeedbackVector(void* object) {
  if (reinterpret_cast<i::Object*>(object)->IsSmi()) {
    printf("Please provide a feedback vector\n");
  } else {
    reinterpret_cast<i::FeedbackVector*>(object)->Print();
  }
}

extern void _v8_internal_Print_DescriptorArray(void* object) {
  if (reinterpret_cast<i::Object*>(object)->IsSmi()) {
    printf("Please provide a descriptor array\n");
  } else {
    reinterpret_cast<i::DescriptorArray*>(object)->Print();
  }
}

extern void _v8_internal_Print_LayoutDescriptor(void* object) {
  i::Object* o = reinterpret_cast<i::Object*>(object);
  if (!o->IsLayoutDescriptor()) {
    printf("Please provide a layout descriptor\n");
  } else {
    reinterpret_cast<i::LayoutDescriptor*>(object)->Print();
  }
}

extern void _v8_internal_Print_TransitionArray(void* object) {
  if (reinterpret_cast<i::Object*>(object)->IsSmi()) {
    printf("Please provide a transition array\n");
  } else {
    reinterpret_cast<i::TransitionArray*>(object)->Print();
  }
}

extern void _v8_internal_Print_StackTrace() {
  i::Isolate* isolate = i::Isolate::Current();
  isolate->PrintStack(stdout);
}

extern void _v8_internal_Print_TransitionTree(void* object) {
  i::Object* o = reinterpret_cast<i::Object*>(object);
  if (!o->IsMap()) {
    printf("Please provide a valid Map\n");
  } else {
#if defined(DEBUG) || defined(OBJECT_PRINT)
    i::DisallowHeapAllocation no_gc;
    i::TransitionsAccessor transitions(reinterpret_cast<i::Map*>(object),
                                       &no_gc);
    transitions.PrintTransitionTree();
#endif
  }
}

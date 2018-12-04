// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects.h"

#include <iomanip>
#include <memory>

#include "src/bootstrapper.h"
#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/instruction-stream.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects-inl.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-break-iterator-inl.h"
#include "src/objects/js-collator-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-collection-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-date-time-format-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-generator-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-list-format-inl.h"
#include "src/objects/js-locale-inl.h"
#include "src/objects/js-number-format-inl.h"
#include "src/objects/js-plural-rules-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-regexp-string-iterator-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-relative-time-format-inl.h"
#include "src/objects/js-segmenter-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/literal-objects-inl.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/microtask-queue-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/promise-inl.h"
#include "src/objects/stack-frame-info-inl.h"
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
  StdoutStream os;
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
  MemoryChunk* chunk = MemoryChunk::FromAddress(
      reinterpret_cast<Address>(const_cast<HeapObject*>(this)));
  if (chunk->owner()->identity() == OLD_SPACE) os << " in OldSpace";
  if (!IsMap()) os << "\n - map: " << Brief(map());
}

void HeapObject::HeapObjectPrint(std::ostream& os) {  // NOLINT
  InstanceType instance_type = map()->instance_type();

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
      MutableHeapNumber::cast(this)->MutableHeapNumberPrint(os);
      os << ">\n";
      break;
    case BIGINT_TYPE:
      BigInt::cast(this)->BigIntPrint(os);
      os << "\n";
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      FixedDoubleArray::cast(this)->FixedDoubleArrayPrint(os);
      break;
    case FIXED_ARRAY_TYPE:
    case AWAIT_CONTEXT_TYPE:
    case BLOCK_CONTEXT_TYPE:
    case CATCH_CONTEXT_TYPE:
    case DEBUG_EVALUATE_CONTEXT_TYPE:
    case EVAL_CONTEXT_TYPE:
    case FUNCTION_CONTEXT_TYPE:
    case MODULE_CONTEXT_TYPE:
    case NATIVE_CONTEXT_TYPE:
    case SCRIPT_CONTEXT_TYPE:
    case WITH_CONTEXT_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
      FixedArray::cast(this)->FixedArrayPrint(os);
      break;
    case HASH_TABLE_TYPE:
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case STRING_TABLE_TYPE:
      ObjectHashTable::cast(this)->ObjectHashTablePrint(os);
      break;
    case NUMBER_DICTIONARY_TYPE:
      NumberDictionary::cast(this)->NumberDictionaryPrint(os);
      break;
    case EPHEMERON_HASH_TABLE_TYPE:
      EphemeronHashTable::cast(this)->EphemeronHashTablePrint(os);
      break;
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
      ObjectBoilerplateDescription::cast(this)
          ->ObjectBoilerplateDescriptionPrint(os);
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
      DescriptorArray::cast(this)->DescriptorArrayPrint(os);
      break;
    case TRANSITION_ARRAY_TYPE:
      TransitionArray::cast(this)->TransitionArrayPrint(os);
      break;
    case FEEDBACK_CELL_TYPE:
      FeedbackCell::cast(this)->FeedbackCellPrint(os);
      break;
    case FEEDBACK_VECTOR_TYPE:
      FeedbackVector::cast(this)->FeedbackVectorPrint(os);
      break;
    case FREE_SPACE_TYPE:
      FreeSpace::cast(this)->FreeSpacePrint(os);
      break;

#define PRINT_FIXED_TYPED_ARRAY(Type, type, TYPE, ctype)      \
  case Fixed##Type##Array::kInstanceType:                     \
    Fixed##Type##Array::cast(this)->FixedTypedArrayPrint(os); \
    break;

      TYPED_ARRAYS(PRINT_FIXED_TYPED_ARRAY)
#undef PRINT_FIXED_TYPED_ARRAY

    case FILLER_TYPE:
      os << "filler";
      break;
    case JS_OBJECT_TYPE:  // fall through
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_ARGUMENTS_TYPE:
    case JS_ERROR_TYPE:
    // TODO(titzer): debug printing for more wasm objects
    case WASM_EXCEPTION_TYPE:
    case WASM_GLOBAL_TYPE:
    case WASM_MEMORY_TYPE:
    case WASM_TABLE_TYPE:
      JSObject::cast(this)->JSObjectPrint(os);
      break;
    case WASM_MODULE_TYPE:
      WasmModuleObject::cast(this)->WasmModuleObjectPrint(os);
      break;
    case WASM_INSTANCE_TYPE:
      WasmInstanceObject::cast(this)->WasmInstanceObjectPrint(os);
      break;
    case JS_GENERATOR_OBJECT_TYPE:
      JSGeneratorObject::cast(this)->JSGeneratorObjectPrint(os);
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
    case JS_REGEXP_STRING_ITERATOR_TYPE:
      JSRegExpStringIterator::cast(this)->JSRegExpStringIteratorPrint(os);
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
    case CALL_HANDLER_INFO_TYPE:
      CallHandlerInfo::cast(this)->CallHandlerInfoPrint(os);
      break;
    case PRE_PARSED_SCOPE_DATA_TYPE:
      PreParsedScopeData::cast(this)->PreParsedScopeDataPrint(os);
      break;
    case UNCOMPILED_DATA_WITHOUT_PRE_PARSED_SCOPE_TYPE:
      UncompiledDataWithoutPreParsedScope::cast(this)
          ->UncompiledDataWithoutPreParsedScopePrint(os);
      break;
    case UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_TYPE:
      UncompiledDataWithPreParsedScope::cast(this)
          ->UncompiledDataWithPreParsedScopePrint(os);
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
    case JS_ARRAY_ITERATOR_TYPE:
      JSArrayIterator::cast(this)->JSArrayIteratorPrint(os);
      break;
    case JS_TYPED_ARRAY_TYPE:
      JSTypedArray::cast(this)->JSTypedArrayPrint(os);
      break;
    case JS_DATA_VIEW_TYPE:
      JSDataView::cast(this)->JSDataViewPrint(os);
      break;
#ifdef V8_INTL_SUPPORT
    case JS_INTL_V8_BREAK_ITERATOR_TYPE:
      JSV8BreakIterator::cast(this)->JSV8BreakIteratorPrint(os);
      break;
    case JS_INTL_COLLATOR_TYPE:
      JSCollator::cast(this)->JSCollatorPrint(os);
      break;
    case JS_INTL_DATE_TIME_FORMAT_TYPE:
      JSDateTimeFormat::cast(this)->JSDateTimeFormatPrint(os);
      break;
    case JS_INTL_LIST_FORMAT_TYPE:
      JSListFormat::cast(this)->JSListFormatPrint(os);
      break;
    case JS_INTL_LOCALE_TYPE:
      JSLocale::cast(this)->JSLocalePrint(os);
      break;
    case JS_INTL_NUMBER_FORMAT_TYPE:
      JSNumberFormat::cast(this)->JSNumberFormatPrint(os);
      break;
    case JS_INTL_PLURAL_RULES_TYPE:
      JSPluralRules::cast(this)->JSPluralRulesPrint(os);
      break;
    case JS_INTL_RELATIVE_TIME_FORMAT_TYPE:
      JSRelativeTimeFormat::cast(this)->JSRelativeTimeFormatPrint(os);
      break;
    case JS_INTL_SEGMENTER_TYPE:
      JSSegmenter::cast(this)->JSSegmenterPrint(os);
      break;
#endif  // V8_INTL_SUPPORT
#define MAKE_STRUCT_CASE(TYPE, Name, name) \
  case TYPE:                               \
    Name::cast(this)->Name##Print(os);     \
    break;
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE

    case ALLOCATION_SITE_TYPE:
      AllocationSite::cast(this)->AllocationSitePrint(os);
      break;
    case LOAD_HANDLER_TYPE:
      LoadHandler::cast(this)->LoadHandlerPrint(os);
      break;
    case STORE_HANDLER_TYPE:
      StoreHandler::cast(this)->StoreHandlerPrint(os);
      break;
    case SCOPE_INFO_TYPE:
      ScopeInfo::cast(this)->ScopeInfoPrint(os);
      break;
    case FEEDBACK_METADATA_TYPE:
      FeedbackMetadata::cast(this)->FeedbackMetadataPrint(os);
      break;
    case WEAK_FIXED_ARRAY_TYPE:
      WeakFixedArray::cast(this)->WeakFixedArrayPrint(os);
      break;
    case WEAK_ARRAY_LIST_TYPE:
      WeakArrayList::cast(this)->WeakArrayListPrint(os);
      break;
    case INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_TYPE:
    case ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case UNCACHED_EXTERNAL_INTERNALIZED_STRING_TYPE:
    case UNCACHED_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case UNCACHED_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case STRING_TYPE:
    case CONS_STRING_TYPE:
    case EXTERNAL_STRING_TYPE:
    case SLICED_STRING_TYPE:
    case THIN_STRING_TYPE:
    case ONE_BYTE_STRING_TYPE:
    case CONS_ONE_BYTE_STRING_TYPE:
    case EXTERNAL_ONE_BYTE_STRING_TYPE:
    case SLICED_ONE_BYTE_STRING_TYPE:
    case THIN_ONE_BYTE_STRING_TYPE:
    case EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case UNCACHED_EXTERNAL_STRING_TYPE:
    case UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE:
    case UNCACHED_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case SMALL_ORDERED_HASH_MAP_TYPE:
    case SMALL_ORDERED_HASH_SET_TYPE:
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
    case JS_STRING_ITERATOR_TYPE:
      // TODO(all): Handle these types too.
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
          os << Brief(descs->GetStrongValue(i));
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
  FixedArray* arguments_store = elements->arguments();
  os << "\n    0: context: " << Brief(elements->context())
     << "\n    1: arguments_store: " << Brief(arguments_store)
     << "\n    parameter to context slot map:";
  for (uint32_t i = 0; i < elements->parameter_map_length(); i++) {
    uint32_t raw_index = i + SloppyArgumentsElements::kParameterMapStart;
    Object* mapped_entry = elements->get_mapped_entry(i);
    os << "\n    " << raw_index << ": param(" << i
       << "): " << Brief(mapped_entry);
    if (mapped_entry->IsTheHole()) {
      os << " in the arguments_store[" << i << "]";
    } else {
      os << " in the context";
    }
  }
  if (arguments_store->length() == 0) return;
  os << "\n }"
     << "\n - arguments_store: " << Brief(arguments_store) << " "
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
  os << " - elements: " << Brief(elements()) << " {";
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

#define PRINT_ELEMENTS(Type, type, TYPE, elementType)    \
  case TYPE##_ELEMENTS: {                                \
    DoPrintElements<Fixed##Type##Array>(os, elements()); \
    break;                                               \
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
  Isolate* isolate = obj->GetIsolate();
  obj->PrintHeader(os, id);
  // Don't call GetElementsKind, its validation code can cause the printer to
  // fail when debugging.
  os << " [";
  if (obj->HasFastProperties()) {
    os << "FastProperties";
  } else {
    os << "DictionaryProperties";
  }
  PrototypeIterator iter(isolate, obj);
  os << "]\n - prototype: " << Brief(iter.GetCurrent());
  os << "\n - elements: " << Brief(obj->elements()) << " ["
     << ElementsKindToString(obj->map()->elements_kind());
  if (obj->elements()->IsCowArray()) os << " (COW)";
  os << "]";
  Object* hash = obj->GetHash();
  if (hash->IsSmi()) {
    os << "\n - hash: " << Brief(hash);
  }
  if (obj->GetEmbedderFieldCount() > 0) {
    os << "\n - embedder fields: " << obj->GetEmbedderFieldCount();
  }
}

static void JSObjectPrintBody(std::ostream& os,
                              JSObject* obj,  // NOLINT
                              bool print_elements = true) {
  os << "\n - properties: ";
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

void JSGeneratorObject::JSGeneratorObjectPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSGeneratorObject");
  os << "\n - function: " << Brief(function());
  os << "\n - context: " << Brief(context());
  os << "\n - receiver: " << Brief(receiver());
  if (is_executing() || is_closed()) {
    os << "\n - input: " << Brief(input_or_debug_pos());
  } else {
    DCHECK(is_suspended());
    os << "\n - debug pos: " << Brief(input_or_debug_pos());
  }
  const char* mode = "(invalid)";
  switch (resume_mode()) {
    case kNext:
      mode = ".next()";
      break;
    case kReturn:
      mode = ".return()";
      break;
    case kThrow:
      mode = ".throw()";
      break;
  }
  os << "\n - resume mode: " << mode;
  os << "\n - continuation: " << continuation();
  if (is_closed()) os << " (closed)";
  if (is_executing()) os << " (executing)";
  if (is_suspended()) os << " (suspended)";
  if (is_suspended()) {
    DisallowHeapAllocation no_gc;
    SharedFunctionInfo* fun_info = function()->shared();
    if (fun_info->HasSourceCode()) {
      Script* script = Script::cast(fun_info->script());
      int lin = script->GetLineNumber(source_position()) + 1;
      int col = script->GetColumnNumber(source_position()) + 1;
      String* script_name = script->name()->IsString()
                                ? String::cast(script->name())
                                : GetReadOnlyRoots().empty_string();
      os << "\n - source position: " << source_position();
      os << " (";
      script_name->PrintUC16(os);
      os << ", lin " << lin;
      os << ", col " << col;
      os << ")";
    }
  }
  os << "\n - register file: " << Brief(parameters_and_registers());
  os << "\n";
}

void JSArray::JSArrayPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSArray");
  os << "\n - length: " << Brief(this->length());
  JSObjectPrintBody(os, this);
}

void JSPromise::JSPromisePrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSPromise");
  os << "\n - status: " << JSPromise::Status(status());
  if (status() == Promise::kPending) {
    os << "\n - reactions: " << Brief(reactions());
  } else {
    os << "\n - result: " << Brief(result());
  }
  os << "\n - has_handler: " << has_handler();
  os << "\n ";
}

void JSRegExp::JSRegExpPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSRegExp");
  os << "\n - data: " << Brief(data());
  os << "\n - source: " << Brief(source());
  JSObjectPrintBody(os, this);
}

void JSRegExpStringIterator::JSRegExpStringIteratorPrint(
    std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSRegExpStringIterator");
  os << "\n - regex: " << Brief(iterating_regexp());
  os << "\n - string: " << Brief(iterating_string());
  os << "\n - done: " << done();
  os << "\n - global: " << global();
  os << "\n - unicode: " << unicode();
  JSObjectPrintBody(os, this);
}

void Symbol::SymbolPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Symbol");
  os << "\n - hash: " << Hash();
  os << "\n - name: " << Brief(name());
  if (name()->IsUndefined()) {
    os << " (" << PrivateSymbolToName() << ")";
  }
  os << "\n - private: " << is_private();
  os << "\n";
}

void Map::MapPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "Map");
  os << "\n - type: " << instance_type();
  os << "\n - instance size: ";
  if (instance_size() == kVariableSizeSentinel) {
    os << "variable";
  } else {
    os << instance_size();
  }
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
  os << "\n - prototype_validity cell: " << Brief(prototype_validity_cell());
  os << "\n - instance descriptors " << (owns_descriptors() ? "(own) " : "")
     << "#" << NumberOfOwnDescriptors() << ": "
     << Brief(instance_descriptors());
  if (FLAG_unbox_double_fields) {
    os << "\n - layout descriptor: ";
    layout_descriptor()->ShortPrint(os);
  }

  Isolate* isolate;
  // Read-only maps can't have transitions, which is fortunate because we need
  // the isolate to iterate over the transitions.
  if (Isolate::FromWritableHeapObject(this, &isolate)) {
    DisallowHeapAllocation no_gc;
    TransitionsAccessor transitions(isolate, this, &no_gc);
    int nof_transitions = transitions.NumberOfTransitions();
    if (nof_transitions > 0) {
      os << "\n - transitions #" << nof_transitions << ": ";
      HeapObject* heap_object;
      Smi* smi;
      if (raw_transitions()->ToSmi(&smi)) {
        os << Brief(smi);
      } else if (raw_transitions()->GetHeapObject(&heap_object)) {
        os << Brief(heap_object);
      }
      transitions.PrintTransitions(os);
    }
  }
  os << "\n - prototype: " << Brief(prototype());
  os << "\n - constructor: " << Brief(GetConstructor());
  os << "\n - dependent code: " << Brief(dependent_code());
  os << "\n - construction counter: " << construction_counter();
  os << "\n";
}

void DescriptorArray::DescriptorArrayPrint(std::ostream& os) {
  HeapObject::PrintHeader(os, "DescriptorArray");
  os << "\n - capacity: " << length();
  EnumCache* enum_cache = GetEnumCache();
  os << "\n - enum_cache: ";
  if (enum_cache->keys()->length() == 0) {
    os << "empty";
  } else {
    os << enum_cache->keys()->length();
    os << "\n   - keys: " << Brief(enum_cache->keys());
    os << "\n   - indices: " << Brief(enum_cache->indices());
  }
  os << "\n - nof descriptors: " << number_of_descriptors();
  PrintDescriptors(os);
}

void AliasedArgumentsEntry::AliasedArgumentsEntryPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "AliasedArgumentsEntry");
  os << "\n - aliased_context_slot: " << aliased_context_slot();
}

namespace {
void PrintFixedArrayWithHeader(std::ostream& os, FixedArray* array,
                               const char* type) {
  array->PrintHeader(os, type);
  os << "\n - length: " << array->length();
  PrintFixedArrayElements(os, array);
  os << "\n";
}

template <typename T>
void PrintHashTableWithHeader(std::ostream& os, T* table, const char* type) {
  table->PrintHeader(os, type);
  os << "\n - length: " << table->length();
  os << "\n - elements: " << table->NumberOfElements();
  os << "\n - deleted: " << table->NumberOfDeletedElements();
  os << "\n - capacity: " << table->Capacity();

  os << "\n - elements: {";
  for (int i = 0; i < table->Capacity(); i++) {
    os << '\n'
       << std::setw(12) << i << ": " << Brief(table->KeyAt(i)) << " -> "
       << Brief(table->ValueAt(i));
  }
  os << "\n }\n";
}

template <typename T>
void PrintWeakArrayElements(std::ostream& os, T* array) {
  // Print in array notation for non-sparse arrays.
  MaybeObject* previous_value = array->length() > 0 ? array->Get(0) : nullptr;
  MaybeObject* value = nullptr;
  int previous_index = 0;
  int i;
  for (i = 1; i <= array->length(); i++) {
    if (i < array->length()) value = array->Get(i);
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

}  // namespace

void FixedArray::FixedArrayPrint(std::ostream& os) {  // NOLINT
  PrintFixedArrayWithHeader(os, this, "FixedArray");
}

void ObjectHashTable::ObjectHashTablePrint(std::ostream& os) {
  PrintHashTableWithHeader(os, this, "ObjectHashTable");
}

void NumberDictionary::NumberDictionaryPrint(std::ostream& os) {
  PrintHashTableWithHeader(os, this, "NumberDictionary");
}

void EphemeronHashTable::EphemeronHashTablePrint(std::ostream& os) {
  PrintHashTableWithHeader(os, this, "EphemeronHashTable");
}

void ObjectBoilerplateDescription::ObjectBoilerplateDescriptionPrint(
    std::ostream& os) {
  PrintFixedArrayWithHeader(os, this, "ObjectBoilerplateDescription");
}

void PropertyArray::PropertyArrayPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PropertyArray");
  os << "\n - length: " << length();
  os << "\n - hash: " << Hash();
  PrintFixedArrayElements(os, this);
  os << "\n";
}

void FixedDoubleArray::FixedDoubleArrayPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "FixedDoubleArray");
  os << "\n - length: " << length();
  DoPrintElements<FixedDoubleArray>(os, this);
  os << "\n";
}

void WeakFixedArray::WeakFixedArrayPrint(std::ostream& os) {
  PrintHeader(os, "WeakFixedArray");
  os << "\n - length: " << length() << "\n";
  PrintWeakArrayElements(os, this);
  os << "\n";
}

void WeakArrayList::WeakArrayListPrint(std::ostream& os) {
  PrintHeader(os, "WeakArrayList");
  os << "\n - capacity: " << capacity();
  os << "\n - length: " << length() << "\n";
  PrintWeakArrayElements(os, this);
  os << "\n";
}

void TransitionArray::TransitionArrayPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "TransitionArray");
  PrintInternal(os);
}

void FeedbackCell::FeedbackCellPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "FeedbackCell");
  ReadOnlyRoots roots = GetReadOnlyRoots();
  if (map() == roots.no_closures_cell_map()) {
    os << "\n - no closures";
  } else if (map() == roots.one_closure_cell_map()) {
    os << "\n - one closure";
  } else if (map() == roots.many_closures_cell_map()) {
    os << "\n - many closures";
  } else {
    os << "\n - Invalid FeedbackCell map";
  }
  os << " - value: " << Brief(value());
  os << "\n";
}

void FeedbackVectorSpec::Print() {
  StdoutStream os;

  FeedbackVectorSpecPrint(os);

  os << std::flush;
}

void FeedbackVectorSpec::FeedbackVectorSpecPrint(std::ostream& os) {  // NOLINT
  int slot_count = slots();
  os << " - slot_count: " << slot_count;
  if (slot_count == 0) {
    os << " (empty)\n";
    return;
  }

  for (int slot = 0; slot < slot_count;) {
    FeedbackSlotKind kind = GetKind(FeedbackSlot(slot));
    int entry_size = FeedbackMetadata::GetSlotSize(kind);
    DCHECK_LT(0, entry_size);
    os << "\n Slot #" << slot << " " << kind;
    slot += entry_size;
  }
  os << "\n";
}

void FeedbackMetadata::FeedbackMetadataPrint(std::ostream& os) {
  HeapObject::PrintHeader(os, "FeedbackMetadata");
  os << "\n - slot_count: " << slot_count();

  FeedbackMetadataIterator iter(this);
  while (iter.HasNext()) {
    FeedbackSlot slot = iter.Next();
    FeedbackSlotKind kind = iter.kind();
    os << "\n Slot " << slot << " " << kind;
  }
  os << "\n";
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
    FeedbackSlotPrint(os, slot);

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
  FeedbackNexus nexus(this, slot);
  nexus.Print(os);
}

void FeedbackNexus::Print(std::ostream& os) {  // NOLINT
  switch (kind()) {
    case FeedbackSlotKind::kCall:
    case FeedbackSlotKind::kLoadProperty:
    case FeedbackSlotKind::kLoadKeyed:
    case FeedbackSlotKind::kLoadGlobalInsideTypeof:
    case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
    case FeedbackSlotKind::kStoreNamedSloppy:
    case FeedbackSlotKind::kStoreNamedStrict:
    case FeedbackSlotKind::kStoreOwnNamed:
    case FeedbackSlotKind::kStoreGlobalSloppy:
    case FeedbackSlotKind::kStoreGlobalStrict:
    case FeedbackSlotKind::kStoreKeyedSloppy:
    case FeedbackSlotKind::kInstanceOf:
    case FeedbackSlotKind::kStoreDataPropertyInLiteral:
    case FeedbackSlotKind::kStoreKeyedStrict:
    case FeedbackSlotKind::kStoreInArrayLiteral:
    case FeedbackSlotKind::kCloneObject: {
      os << InlineCacheState2String(StateFromFeedback());
      break;
    }
    case FeedbackSlotKind::kBinaryOp: {
      os << "BinaryOp:" << GetBinaryOperationFeedback();
      break;
    }
    case FeedbackSlotKind::kCompareOp: {
      os << "CompareOp:" << GetCompareOperationFeedback();
      break;
    }
    case FeedbackSlotKind::kForIn: {
      os << "ForIn:" << GetForInFeedback();
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
  os << "\n - value: " << Brief(value());
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
  os << "\n - value: " << Brief(value());
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
  os << "\n - target: ";
  target()->ShortPrint(os);
  os << "\n - handler: ";
  handler()->ShortPrint(os);
  os << "\n";
}

void JSSet::JSSetPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSSet");
  os << " - table: " << Brief(table());
  JSObjectPrintBody(os, this);
}

void JSMap::JSMapPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSMap");
  os << " - table: " << Brief(table());
  JSObjectPrintBody(os, this);
}

void JSCollectionIterator::JSCollectionIteratorPrint(
    std::ostream& os) {  // NOLINT
  os << "\n - table: " << Brief(table());
  os << "\n - index: " << Brief(index());
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
  os << "\n - table: " << Brief(table());
  JSObjectPrintBody(os, this);
}

void JSWeakSet::JSWeakSetPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSWeakSet");
  os << "\n - table: " << Brief(table());
  JSObjectPrintBody(os, this);
}

void JSArrayBuffer::JSArrayBufferPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSArrayBuffer");
  os << "\n - backing_store: " << backing_store();
  os << "\n - byte_length: " << byte_length();
  if (is_external()) os << "\n - external";
  if (is_neuterable()) os << "\n - neuterable";
  if (was_neutered()) os << "\n - neutered";
  if (is_shared()) os << "\n - shared";
  if (is_wasm_memory()) os << "\n - is_wasm_memory";
  if (is_growable()) os << "\n - growable";
  JSObjectPrintBody(os, this, !was_neutered());
}

void JSTypedArray::JSTypedArrayPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSTypedArray");
  os << "\n - buffer: " << Brief(buffer());
  os << "\n - byte_offset: " << byte_offset();
  os << "\n - byte_length: " << byte_length();
  os << "\n - length: " << Brief(length());
  if (WasNeutered()) os << "\n - neutered";
  JSObjectPrintBody(os, this, !WasNeutered());
}

void JSArrayIterator::JSArrayIteratorPrint(std::ostream& os) {  // NOLING
  JSObjectPrintHeader(os, this, "JSArrayIterator");
  os << "\n - iterated_object: " << Brief(iterated_object());
  os << "\n - next_index: " << Brief(next_index());
  os << "\n - kind: " << kind();
  JSObjectPrintBody(os, this);
}

void JSDataView::JSDataViewPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSDataView");
  os << "\n - buffer =" << Brief(buffer());
  os << "\n - byte_offset: " << byte_offset();
  os << "\n - byte_length: " << byte_length();
  if (WasNeutered()) os << "\n - neutered";
  JSObjectPrintBody(os, this, !WasNeutered());
}

void JSBoundFunction::JSBoundFunctionPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSBoundFunction");
  os << "\n - bound_target_function: " << Brief(bound_target_function());
  os << "\n - bound_this: " << Brief(bound_this());
  os << "\n - bound_arguments: " << Brief(bound_arguments());
  JSObjectPrintBody(os, this);
}

void JSFunction::JSFunctionPrint(std::ostream& os) {  // NOLINT
  Isolate* isolate = GetIsolate();
  JSObjectPrintHeader(os, this, "Function");
  os << "\n - function prototype: ";
  if (has_prototype_slot()) {
    if (has_prototype()) {
      os << Brief(prototype());
      if (map()->has_non_instance_prototype()) {
        os << " (non-instance prototype)";
      }
    }
    os << "\n - initial_map: ";
    if (has_initial_map()) os << Brief(initial_map());
  } else {
    os << "<no-prototype-slot>";
  }
  os << "\n - shared_info: " << Brief(shared());
  os << "\n - name: " << Brief(shared()->Name());

  // Print Builtin name for builtin functions
  int builtin_index = code()->builtin_index();
  if (builtin_index != -1 && !IsInterpreted()) {
    if (builtin_index == Builtins::kDeserializeLazy) {
      if (shared()->HasBuiltinId()) {
        builtin_index = shared()->builtin_id();
        os << "\n - builtin: " << isolate->builtins()->name(builtin_index)
           << "(lazy)";
      }
    } else {
      os << "\n - builtin: " << isolate->builtins()->name(builtin_index);
    }
  }

  os << "\n - formal_parameter_count: "
     << shared()->internal_formal_parameter_count();
  os << "\n - kind: " << shared()->kind();
  os << "\n - context: " << Brief(context());
  os << "\n - code: " << Brief(code());
  if (IsInterpreted()) {
    os << "\n - interpreted";
    if (shared()->HasBytecodeArray()) {
      os << "\n - bytecode: " << shared()->GetBytecodeArray();
    }
  }
  if (WasmExportedFunction::IsWasmExportedFunction(this)) {
    WasmExportedFunction* function = WasmExportedFunction::cast(this);
    os << "\n - WASM instance "
       << reinterpret_cast<void*>(function->instance());
    os << "\n - WASM function index " << function->function_index();
  }
  shared()->PrintSourceCode(os);
  JSObjectPrintBody(os, this);
  os << "\n - feedback vector: ";
  if (!shared()->HasFeedbackMetadata()) {
    os << "feedback metadata is not available in SFI\n";
  } else if (has_feedback_vector()) {
    feedback_vector()->FeedbackVectorPrint(os);
  } else {
    os << "not available\n";
  }
}

void SharedFunctionInfo::PrintSourceCode(std::ostream& os) {
  if (HasSourceCode()) {
    os << "\n - source code: ";
    String* source = String::cast(Script::cast(script())->source());
    int start = StartPosition();
    int length = EndPosition() - start;
    std::unique_ptr<char[]> source_string = source->ToCString(
        DISALLOW_NULLS, FAST_STRING_TRAVERSAL, start, length, nullptr);
    os << source_string.get();
  }
}

void SharedFunctionInfo::SharedFunctionInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "SharedFunctionInfo");
  os << "\n - name: ";
  if (HasSharedName()) {
    os << Brief(Name());
  } else {
    os << "<no-shared-name>";
  }
  if (HasInferredName()) {
    os << "\n - inferred name: " << Brief(inferred_name());
  }
  os << "\n - kind: " << kind();
  if (needs_home_object()) {
    os << "\n - needs_home_object";
  }
  os << "\n - function_map_index: " << function_map_index();
  os << "\n - formal_parameter_count: " << internal_formal_parameter_count();
  os << "\n - expected_nof_properties: " << expected_nof_properties();
  os << "\n - language_mode: " << language_mode();
  os << "\n - data: " << Brief(function_data());
  os << "\n - code (from data): " << Brief(GetCode());
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
  os << "\n - function token position: " << function_token_position();
  os << "\n - start position: " << StartPosition();
  os << "\n - end position: " << EndPosition();
  if (HasDebugInfo()) {
    os << "\n - debug info: " << Brief(GetDebugInfo());
  } else {
    os << "\n - no debug info";
  }
  os << "\n - scope info: " << Brief(scope_info());
  if (HasOuterScopeInfo()) {
    os << "\n - outer scope info: " << Brief(GetOuterScopeInfo());
  }
  os << "\n - length: " << length();
  os << "\n - feedback_metadata: ";
  if (HasFeedbackMetadata()) {
    feedback_metadata()->FeedbackMetadataPrint(os);
  } else {
    os << "<none>";
  }
  os << "\n";
}

void JSGlobalProxy::JSGlobalProxyPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSGlobalProxy");
  if (!GetIsolate()->bootstrapper()->IsActive()) {
    os << "\n - native context: " << Brief(native_context());
  }
  JSObjectPrintBody(os, this);
}

void JSGlobalObject::JSGlobalObjectPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSGlobalObject");
  if (!GetIsolate()->bootstrapper()->IsActive()) {
    os << "\n - native context: " << Brief(native_context());
  }
  os << "\n - global proxy: " << Brief(global_proxy());
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
  if (value()->IsTheHole()) {
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

void CallbackTask::CallbackTaskPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "CallbackTask");
  os << "\n - callback: " << Brief(callback());
  os << "\n - data: " << Brief(data());
  os << "\n";
}

void CallableTask::CallableTaskPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "CallableTask");
  os << "\n - context: " << Brief(context());
  os << "\n - callable: " << Brief(callable());
  os << "\n";
}

void PromiseFulfillReactionJobTask::PromiseFulfillReactionJobTaskPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PromiseFulfillReactionJobTask");
  os << "\n - argument: " << Brief(argument());
  os << "\n - context: " << Brief(context());
  os << "\n - handler: " << Brief(handler());
  os << "\n - promise_or_capability: " << Brief(promise_or_capability());
  os << "\n";
}

void PromiseRejectReactionJobTask::PromiseRejectReactionJobTaskPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PromiseRejectReactionJobTask");
  os << "\n - argument: " << Brief(argument());
  os << "\n - context: " << Brief(context());
  os << "\n - handler: " << Brief(handler());
  os << "\n - promise_or_capability: " << Brief(promise_or_capability());
  os << "\n";
}

void PromiseResolveThenableJobTask::PromiseResolveThenableJobTaskPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PromiseResolveThenableJobTask");
  os << "\n - context: " << Brief(context());
  os << "\n - promise_to_resolve: " << Brief(promise_to_resolve());
  os << "\n - then: " << Brief(then());
  os << "\n - thenable: " << Brief(thenable());
  os << "\n";
}

void PromiseCapability::PromiseCapabilityPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PromiseCapability");
  os << "\n - promise: " << Brief(promise());
  os << "\n - resolve: " << Brief(resolve());
  os << "\n - reject: " << Brief(reject());
  os << "\n";
}

void PromiseReaction::PromiseReactionPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "PromiseReaction");
  os << "\n - next: " << Brief(next());
  os << "\n - reject_handler: " << Brief(reject_handler());
  os << "\n - fulfill_handler: " << Brief(fulfill_handler());
  os << "\n - promise_or_capability: " << Brief(promise_or_capability());
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
  os << "\n - module namespace: " << Brief(module_namespace());
  os << "\n - prototype users: " << Brief(prototype_users());
  os << "\n - registry slot: " << registry_slot();
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

void ArrayBoilerplateDescription::ArrayBoilerplateDescriptionPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "ArrayBoilerplateDescription");
  os << "\n - elements kind: " << elements_kind();
  os << "\n - constant elements: " << Brief(constant_elements());
  os << "\n";
}

void WasmDebugInfo::WasmDebugInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "WasmDebugInfo");
  os << "\n - wasm_instance: " << Brief(wasm_instance());
  os << "\n";
}

void WasmInstanceObject::WasmInstanceObjectPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "WasmInstanceObject");
  os << "\n - module_object: " << Brief(module_object());
  os << "\n - exports_object: " << Brief(exports_object());
  os << "\n - native_context: " << Brief(native_context());
  if (has_memory_object()) {
    os << "\n - memory_object: " << Brief(memory_object());
  }
  if (has_globals_buffer()) {
    os << "\n - globals_buffer: " << Brief(globals_buffer());
  }
  if (has_imported_mutable_globals_buffers()) {
    os << "\n - imported_mutable_globals_buffers: "
       << Brief(imported_mutable_globals_buffers());
  }
  if (has_debug_info()) {
    os << "\n - debug_info: " << Brief(debug_info());
  }
  if (has_table_object()) {
    os << "\n - table_object: " << Brief(table_object());
  }
  os << "\n - imported_function_instances: "
     << Brief(imported_function_instances());
  os << "\n - imported_function_callables: "
     << Brief(imported_function_callables());
  if (has_indirect_function_table_instances()) {
    os << "\n - indirect_function_table_instances: "
       << Brief(indirect_function_table_instances());
  }
  if (has_managed_native_allocations()) {
    os << "\n - managed_native_allocations: "
       << Brief(managed_native_allocations());
  }
  os << "\n - memory_start: " << static_cast<void*>(memory_start());
  os << "\n - memory_size: " << memory_size();
  os << "\n - memory_mask: " << AsHex(memory_mask());
  os << "\n - imported_function_targets: "
     << static_cast<void*>(imported_function_targets());
  os << "\n - globals_start: " << static_cast<void*>(globals_start());
  os << "\n - imported_mutable_globals: "
     << static_cast<void*>(imported_mutable_globals());
  os << "\n - indirect_function_table_size: " << indirect_function_table_size();
  os << "\n - indirect_function_table_sig_ids: "
     << static_cast<void*>(indirect_function_table_sig_ids());
  os << "\n - indirect_function_table_targets: "
     << static_cast<void*>(indirect_function_table_targets());
  os << "\n";
}

void WasmExportedFunctionData::WasmExportedFunctionDataPrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "WasmExportedFunctionData");
  os << "\n - wrapper_code: " << Brief(wrapper_code());
  os << "\n - instance: " << Brief(instance());
  os << "\n - function_index: " << function_index();
  os << "\n";
}

void WasmModuleObject::WasmModuleObjectPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "WasmModuleObject");
  os << "\n - module: " << module();
  os << "\n - native module: " << native_module();
  os << "\n - export wrappers: " << Brief(export_wrappers());
  os << "\n - script: " << Brief(script());
  if (has_asm_js_offset_table()) {
    os << "\n - asm_js_offset_table: " << Brief(asm_js_offset_table());
  }
  if (has_breakpoint_infos()) {
    os << "\n - breakpoint_infos: " << Brief(breakpoint_infos());
  }
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

void CallHandlerInfo::CallHandlerInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "CallHandlerInfo");
  os << "\n - callback: " << Brief(callback());
  os << "\n - js_callback: " << Brief(js_callback());
  os << "\n - data: " << Brief(data());
  os << "\n - side_effect_free: "
     << (IsSideEffectFreeCallHandlerInfo() ? "true" : "false");
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
  if (this->HasWeakNext()) os << "\n - weak_next: " << Brief(weak_next());
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
    GetAllocationSite()->AllocationSitePrint(os);
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

#ifdef V8_INTL_SUPPORT
void JSV8BreakIterator::JSV8BreakIteratorPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSV8BreakIterator");
  os << "\n - locale: " << Brief(locale());
  os << "\n - type: " << TypeAsString();
  os << "\n - break iterator: " << Brief(break_iterator());
  os << "\n - unicode string: " << Brief(unicode_string());
  os << "\n - bound adopt text: " << Brief(bound_adopt_text());
  os << "\n - bound first: " << Brief(bound_first());
  os << "\n - bound next: " << Brief(bound_next());
  os << "\n - bound current: " << Brief(bound_current());
  os << "\n - bound break type: " << Brief(bound_break_type());
  os << "\n";
}

void JSCollator::JSCollatorPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSCollator");
  os << "\n - icu collator: " << Brief(icu_collator());
  os << "\n - bound compare: " << Brief(bound_compare());
  os << "\n";
}

void JSDateTimeFormat::JSDateTimeFormatPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSDateTimeFormat");
  os << "\n - icu locale: " << Brief(icu_locale());
  os << "\n - icu simple date format: " << Brief(icu_simple_date_format());
  os << "\n - bound format: " << Brief(bound_format());
  os << "\n";
}

void JSListFormat::JSListFormatPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSListFormat");
  os << "\n - locale: " << Brief(locale());
  os << "\n - style: " << StyleAsString();
  os << "\n - type: " << TypeAsString();
  os << "\n - icu formatter: " << Brief(icu_formatter());
  os << "\n";
}

void JSLocale::JSLocalePrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSLocale");
  os << "\n - language: " << Brief(language());
  os << "\n - script: " << Brief(script());
  os << "\n - region: " << Brief(region());
  os << "\n - baseName: " << Brief(base_name());
  os << "\n - locale: " << Brief(locale());
  os << "\n - calendar: " << Brief(calendar());
  os << "\n - caseFirst: " << CaseFirstAsString();
  os << "\n - collation: " << Brief(collation());
  os << "\n - hourCycle: " << HourCycleAsString();
  os << "\n - numeric: " << NumericAsString();
  os << "\n - numberingSystem: " << Brief(numbering_system());
  os << "\n";
}

void JSNumberFormat::JSNumberFormatPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSNumberFormat");
  os << "\n - locale: " << Brief(locale());
  os << "\n - icu_number_format: " << Brief(icu_number_format());
  os << "\n - bound_format: " << Brief(bound_format());
  os << "\n - style: " << StyleAsString();
  os << "\n - currency_display: " << CurrencyDisplayAsString();
  os << "\n";
}

void JSPluralRules::JSPluralRulesPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "JSPluralRules");
  JSObjectPrint(os);
  os << "\n - locale: " << Brief(locale());
  os << "\n - type: " << Brief(type());
  os << "\n - icu plural rules: " << Brief(icu_plural_rules());
  os << "\n - icu decimal format: " << Brief(icu_decimal_format());
  os << "\n";
}

void JSRelativeTimeFormat::JSRelativeTimeFormatPrint(
    std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSRelativeTimeFormat");
  os << "\n - locale: " << Brief(locale());
  os << "\n - style: " << StyleAsString();
  os << "\n - numeric: " << NumericAsString();
  os << "\n - icu formatter: " << Brief(icu_formatter());
  os << "\n";
}

void JSSegmenter::JSSegmenterPrint(std::ostream& os) {  // NOLINT
  JSObjectPrintHeader(os, this, "JSSegmenter");
  os << "\n - locale: " << Brief(locale());
  os << "\n - granularity: " << GranularityAsString();
  os << "\n - lineBreakStyle: " << LineBreakStyleAsString();
  os << "\n - icubreak iterator: " << Brief(icu_break_iterator());
  os << "\n";
}
#endif  // V8_INTL_SUPPORT

namespace {
void PrintScopeInfoList(ScopeInfo* scope_info, std::ostream& os,
                        const char* list_name, int nof_internal_slots,
                        int start, int length) {
  if (length <= 0) return;
  int end = start + length;
  os << "\n - " << list_name;
  if (nof_internal_slots > 0) {
    os << " " << start << "-" << end << " [internal slots]";
  }
  os << " {\n";
  for (int i = nof_internal_slots; start < end; ++i, ++start) {
    os << "    - " << i << ": ";
    String::cast(scope_info->get(start))->ShortPrint(os);
    os << "\n";
  }
  os << "  }";
}
}  // namespace

void ScopeInfo::ScopeInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "ScopeInfo");
  if (length() == 0) {
    os << "\n - length = 0\n";
    return;
  }
  int flags = Flags();

  os << "\n - parameters: " << ParameterCount();
  os << "\n - context locals : " << ContextLocalCount();

  os << "\n - scope type: " << scope_type();
  if (CallsSloppyEval()) os << "\n - sloppy eval";
  os << "\n - language mode: " << language_mode();
  if (is_declaration_scope()) os << "\n - declaration scope";
  if (HasReceiver()) {
    os << "\n - receiver: " << ReceiverVariableField::decode(flags);
  }
  if (HasNewTarget()) os << "\n - needs new target";
  if (HasFunctionName()) {
    os << "\n - function name(" << FunctionVariableField::decode(flags)
       << "): ";
    FunctionName()->ShortPrint(os);
  }
  if (IsAsmModule()) os << "\n - asm module";
  if (HasSimpleParameters()) os << "\n - simple parameters";
  os << "\n - function kind: " << function_kind();
  if (HasOuterScopeInfo()) {
    os << "\n - outer scope info: " << Brief(OuterScopeInfo());
  }
  if (HasFunctionName()) {
    os << "\n - function name: " << Brief(FunctionName());
  }
  if (HasInferredFunctionName()) {
    os << "\n - inferred function name: " << Brief(InferredFunctionName());
  }

  if (HasPositionInfo()) {
    os << "\n - start position: " << StartPosition();
    os << "\n - end position: " << EndPosition();
  }
  os << "\n - length: " << length();
  if (length() > 0) {
    PrintScopeInfoList(this, os, "context slots", Context::MIN_CONTEXT_SLOTS,
                       ContextLocalNamesIndex(), ContextLocalCount());
    // TODO(neis): Print module stuff if present.
  }
  os << "\n";
}

void DebugInfo::DebugInfoPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "DebugInfo");
  os << "\n - flags: " << flags();
  os << "\n - debugger_hints: " << debugger_hints();
  os << "\n - shared: " << Brief(shared());
  os << "\n - script: " << Brief(script());
  os << "\n - original bytecode array: " << Brief(original_bytecode_array());
  os << "\n - break_points: ";
  break_points()->FixedArrayPrint(os);
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
  StdoutStream os;
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
  } else if (IsOddball() && IsUninitialized()) {
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
  os << "\n - length: " << length();
  for (int i = 0; i < length(); ++i) {
    os << "\n - [" << i << "]: " << Brief(child_data(i));
  }
  os << "\n";
}

void UncompiledDataWithoutPreParsedScope::
    UncompiledDataWithoutPreParsedScopePrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "UncompiledDataWithoutPreParsedScope");
  os << "\n - start position: " << start_position();
  os << "\n - end position: " << end_position();
  os << "\n";
}

void UncompiledDataWithPreParsedScope::UncompiledDataWithPreParsedScopePrint(
    std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "UncompiledDataWithPreParsedScope");
  os << "\n - start position: " << start_position();
  os << "\n - end position: " << end_position();
  os << "\n - pre_parsed_scope_data: " << Brief(pre_parsed_scope_data());
  os << "\n";
}

void MicrotaskQueue::MicrotaskQueuePrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "MicrotaskQueue");
  os << "\n - pending_microtask_count: " << pending_microtask_count();
  os << "\n - queue: " << Brief(queue());
  os << "\n";
}

void InterpreterData::InterpreterDataPrint(std::ostream& os) {  // NOLINT
  HeapObject::PrintHeader(os, "InterpreterData");
  os << "\n - bytecode_array: " << Brief(bytecode_array());
  os << "\n - interpreter_trampoline: " << Brief(interpreter_trampoline());
  os << "\n";
}

void MaybeObject::Print() {
  StdoutStream os;
  this->Print(os);
  os << std::flush;
}

void MaybeObject::Print(std::ostream& os) {
  Smi* smi;
  HeapObject* heap_object;
  if (ToSmi(&smi)) {
    smi->SmiPrint(os);
  } else if (IsCleared()) {
    os << "[cleared]";
  } else if (GetHeapObjectIfWeak(&heap_object)) {
    os << "[weak] ";
    heap_object->HeapObjectPrint(os);
  } else if (GetHeapObjectIfStrong(&heap_object)) {
    heap_object->HeapObjectPrint(os);
  } else {
    UNREACHABLE();
  }
}

#endif  // OBJECT_PRINT

void HeapNumber::HeapNumberPrint(std::ostream& os) { os << value(); }

void MutableHeapNumber::MutableHeapNumberPrint(std::ostream& os) {
  os << value();
}

// TODO(cbruni): remove once the new maptracer is in place.
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

// TODO(cbruni): remove once the new maptracer is in place.
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

void Map::PrintMapDetails(std::ostream& os) {
  DisallowHeapAllocation no_gc;
#ifdef OBJECT_PRINT
  this->MapPrint(os);
#else
  os << "Map=" << reinterpret_cast<void*>(this);
#endif
  os << "\n";
  instance_descriptors()->PrintDescriptors(os);
}

void DescriptorArray::PrintDescriptors(std::ostream& os) {
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
  switch (details.location()) {
    case kField: {
      FieldType* field_type = GetFieldType(descriptor);
      field_type->PrintTo(os);
      break;
    }
    case kDescriptor:
      Object* value = GetStrongValue(descriptor);
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

// static
void TransitionsAccessor::PrintOneTransition(std::ostream& os, Name* key,
                                             Map* target) {
  os << "\n     ";
#ifdef OBJECT_PRINT
  key->NamePrint(os);
#else
  key->ShortPrint(os);
#endif
  os << ": ";
  ReadOnlyRoots roots = key->GetReadOnlyRoots();
  if (key == roots.nonextensible_symbol()) {
    os << "(transition to non-extensible)";
  } else if (key == roots.sealed_symbol()) {
    os << "(transition to sealed)";
  } else if (key == roots.frozen_symbol()) {
    os << "(transition to frozen)";
  } else if (key == roots.elements_transition_symbol()) {
    os << "(transition to " << ElementsKindToString(target->elements_kind())
       << ")";
  } else if (key == roots.strict_function_transition_symbol()) {
    os << " (transition to strict function)";
  } else {
    DCHECK(!IsSpecialTransition(roots, key));
    os << "(transition to ";
    int descriptor = target->LastAdded();
    DescriptorArray* descriptors = target->instance_descriptors();
    descriptors->PrintDescriptorDetails(os, descriptor,
                                        PropertyDetails::kForTransitions);
    os << ")";
  }
  os << " -> " << Brief(target);
}

void TransitionArray::PrintInternal(std::ostream& os) {
  int num_transitions = number_of_transitions();
  os << "Transition array #" << num_transitions << ":";
  for (int i = 0; i < num_transitions; i++) {
    Name* key = GetKey(i);
    Map* target = GetTarget(i);
    TransitionsAccessor::PrintOneTransition(os, key, target);
  }
  os << "\n" << std::flush;
}

void TransitionsAccessor::PrintTransitions(std::ostream& os) {  // NOLINT
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
      return;
    case kWeakRef: {
      Map* target = Map::cast(raw_transitions_->GetHeapObjectAssumeWeak());
      Name* key = GetSimpleTransitionKey(target);
      PrintOneTransition(os, key, target);
      break;
    }
    case kFullTransitionArray:
      return transitions()->PrintInternal(os);
  }
}

void TransitionsAccessor::PrintTransitionTree() {
  StdoutStream os;
  os << "map= " << Brief(map_);
  DisallowHeapAllocation no_gc;
  PrintTransitionTree(os, 0, &no_gc);
  os << "\n" << std::flush;
}

void TransitionsAccessor::PrintTransitionTree(std::ostream& os, int level,
                                              DisallowHeapAllocation* no_gc) {
  ReadOnlyRoots roots = ReadOnlyRoots(isolate_);
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

    if (key == roots.nonextensible_symbol()) {
      os << "to non-extensible";
    } else if (key == roots.sealed_symbol()) {
      os << "to sealed ";
    } else if (key == roots.frozen_symbol()) {
      os << "to frozen";
    } else if (key == roots.elements_transition_symbol()) {
      os << "to " << ElementsKindToString(target->elements_kind());
    } else if (key == roots.strict_function_transition_symbol()) {
      os << "to strict function";
    } else {
#ifdef OBJECT_PRINT
      key->NamePrint(os);
#else
      key->ShortPrint(os);
#endif
      os << " ";
      DCHECK(!IsSpecialTransition(ReadOnlyRoots(isolate_), key));
      os << "to ";
      int descriptor = target->LastAdded();
      DescriptorArray* descriptors = target->instance_descriptors();
      descriptors->PrintDescriptorDetails(os, descriptor,
                                          PropertyDetails::kForTransitions);
    }
    TransitionsAccessor transitions(isolate_, target, no_gc);
    transitions.PrintTransitionTree(os, level + 1, no_gc);
  }
}

void JSObject::PrintTransitions(std::ostream& os) {  // NOLINT
  DisallowHeapAllocation no_gc;
  TransitionsAccessor ta(GetIsolate(), map(), &no_gc);
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
V8_EXPORT_PRIVATE extern void _v8_internal_Print_Object(void* object) {
  reinterpret_cast<i::Object*>(object)->Print();
}

V8_EXPORT_PRIVATE extern void _v8_internal_Print_Code(void* object) {
  i::Address address = reinterpret_cast<i::Address>(object);
  i::Isolate* isolate = i::Isolate::Current();

  i::wasm::WasmCode* wasm_code =
      isolate->wasm_engine()->code_manager()->LookupCode(address);
  if (wasm_code) {
    i::StdoutStream os;
    wasm_code->Disassemble(nullptr, os, address);
    return;
  }

  if (!isolate->heap()->InSpaceSlow(address, i::CODE_SPACE) &&
      !isolate->heap()->InSpaceSlow(address, i::LO_SPACE) &&
      !i::InstructionStream::PcIsOffHeap(isolate, address)) {
    i::PrintF(
        "%p is not within the current isolate's large object, code or embedded "
        "spaces\n",
        object);
    return;
  }

  i::Code* code = isolate->FindCodeObject(address);
  if (!code->IsCode()) {
    i::PrintF("No code object found containing %p\n", object);
    return;
  }
#ifdef ENABLE_DISASSEMBLER
  i::StdoutStream os;
  code->Disassemble(nullptr, os, address);
#else   // ENABLE_DISASSEMBLER
  code->Print();
#endif  // ENABLE_DISASSEMBLER
}

V8_EXPORT_PRIVATE extern void _v8_internal_Print_LayoutDescriptor(
    void* object) {
  i::Object* o = reinterpret_cast<i::Object*>(object);
  if (!o->IsLayoutDescriptor()) {
    printf("Please provide a layout descriptor\n");
  } else {
    reinterpret_cast<i::LayoutDescriptor*>(object)->Print();
  }
}

V8_EXPORT_PRIVATE extern void _v8_internal_Print_StackTrace() {
  i::Isolate* isolate = i::Isolate::Current();
  isolate->PrintStack(stdout);
}

V8_EXPORT_PRIVATE extern void _v8_internal_Print_TransitionTree(void* object) {
  i::Object* o = reinterpret_cast<i::Object*>(object);
  if (!o->IsMap()) {
    printf("Please provide a valid Map\n");
  } else {
#if defined(DEBUG) || defined(OBJECT_PRINT)
    i::DisallowHeapAllocation no_gc;
    i::Map* map = reinterpret_cast<i::Map*>(object);
    i::TransitionsAccessor transitions(i::Isolate::Current(), map, &no_gc);
    transitions.PrintTransitionTree();
#endif
  }
}

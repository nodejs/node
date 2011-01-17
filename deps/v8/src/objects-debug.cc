// Copyright 2010 the V8 project authors. All rights reserved.
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


void CodeCache::CodeCacheVerify() {
  VerifyHeapPointer(default_cache());
  VerifyHeapPointer(normal_type_cache());
  ASSERT(default_cache()->IsFixedArray());
  ASSERT(normal_type_cache()->IsUndefined()
         || normal_type_cache()->IsCodeCacheHashTable());
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


void JSValue::JSValueVerify() {
  Object* v = value();
  if (v->IsHeapObject()) {
    VerifyHeapPointer(v);
  }
}


void String::StringVerify() {
  CHECK(IsString());
  CHECK(length() >= 0 && length() <= Smi::kMaxValue);
  if (IsSymbol()) {
    CHECK(!Heap::InNewSpace(this));
  }
}


void JSFunction::JSFunctionVerify() {
  CHECK(IsJSFunction());
  VerifyObjectField(kPrototypeOrInitialMapOffset);
  VerifyObjectField(kNextFunctionLinkOffset);
  CHECK(next_function_link()->IsUndefined() ||
        next_function_link()->IsJSFunction());
}


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


void JSGlobalProxy::JSGlobalProxyVerify() {
  CHECK(IsJSGlobalProxy());
  JSObjectVerify();
  VerifyObjectField(JSGlobalProxy::kContextOffset);
  // Make sure that this object has no properties, elements.
  CHECK_EQ(0, properties()->length());
  CHECK(HasFastElements());
  CHECK_EQ(0, FixedArray::cast(elements())->length());
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
    // Hidden oddballs have negative smis.
    const int kLeastHiddenOddballNumber = -4;
    ASSERT(value <= 1);
    ASSERT(value >= kLeastHiddenOddballNumber);
  }
}


void JSGlobalPropertyCell::JSGlobalPropertyCellVerify() {
  CHECK(IsJSGlobalPropertyCell());
  VerifyObjectField(kValueOffset);
}


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


void AccessCheckInfo::AccessCheckInfoVerify() {
  CHECK(IsAccessCheckInfo());
  VerifyPointer(named_callback());
  VerifyPointer(indexed_callback());
  VerifyPointer(data());
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


void CallHandlerInfo::CallHandlerInfoVerify() {
  CHECK(IsCallHandlerInfo());
  VerifyPointer(callback());
  VerifyPointer(data());
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


void ObjectTemplateInfo::ObjectTemplateInfoVerify() {
  CHECK(IsObjectTemplateInfo());
  TemplateInfoVerify();
  VerifyPointer(constructor());
  VerifyPointer(internal_field_count());
}


void SignatureInfo::SignatureInfoVerify() {
  CHECK(IsSignatureInfo());
  VerifyPointer(receiver());
  VerifyPointer(args());
}


void TypeSwitchInfo::TypeSwitchInfoVerify() {
  CHECK(IsTypeSwitchInfo());
  VerifyPointer(types());
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


#ifdef ENABLE_DEBUGGER_SUPPORT
void DebugInfo::DebugInfoVerify() {
  CHECK(IsDebugInfo());
  VerifyPointer(shared());
  VerifyPointer(original_code());
  VerifyPointer(code());
  VerifyPointer(break_points());
}


void BreakPointInfo::BreakPointInfoVerify() {
  CHECK(IsBreakPointInfo());
  code_position()->SmiVerify();
  source_position()->SmiVerify();
  statement_position()->SmiVerify();
  VerifyPointer(break_point_objects());
}
#endif  // ENABLE_DEBUGGER_SUPPORT


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
    STATIC_ASSERT(2 == kEntrySize);
    for (int i = kEntriesIndex; i < length(); i += kEntrySize) {
      get(i)->Verify();
      get(i + 1)->Verify();
      // Key and value must be either both the holes, or not.
      ASSERT(get(i)->IsTheHole() == get(i + 1)->IsTheHole());
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

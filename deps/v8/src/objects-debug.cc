// Copyright 2012 the V8 project authors. All rights reserved.
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
#include "macro-assembler.h"
#include "objects-visiting.h"

namespace v8 {
namespace internal {

#ifdef VERIFY_HEAP

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
    CHECK(p->IsSmi());
  }
}


void Smi::SmiVerify() {
  CHECK(IsSmi());
}


void Failure::FailureVerify() {
  CHECK(IsFailure());
}


void HeapObject::HeapObjectVerify() {
  InstanceType instance_type = map()->instance_type();

  if (instance_type < FIRST_NONSTRING_TYPE) {
    String::cast(this)->StringVerify();
    return;
  }

  switch (instance_type) {
    case SYMBOL_TYPE:
      Symbol::cast(this)->SymbolVerify();
      break;
    case MAP_TYPE:
      Map::cast(this)->MapVerify();
      break;
    case HEAP_NUMBER_TYPE:
      HeapNumber::cast(this)->HeapNumberVerify();
      break;
    case FIXED_ARRAY_TYPE:
      FixedArray::cast(this)->FixedArrayVerify();
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      FixedDoubleArray::cast(this)->FixedDoubleArrayVerify();
      break;
    case BYTE_ARRAY_TYPE:
      ByteArray::cast(this)->ByteArrayVerify();
      break;
    case FREE_SPACE_TYPE:
      FreeSpace::cast(this)->FreeSpaceVerify();
      break;
    case EXTERNAL_PIXEL_ARRAY_TYPE:
      ExternalPixelArray::cast(this)->ExternalPixelArrayVerify();
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
    case EXTERNAL_DOUBLE_ARRAY_TYPE:
      ExternalDoubleArray::cast(this)->ExternalDoubleArrayVerify();
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
    case JS_GENERATOR_OBJECT_TYPE:
      JSGeneratorObject::cast(this)->JSGeneratorObjectVerify();
      break;
    case JS_MODULE_TYPE:
      JSModule::cast(this)->JSModuleVerify();
      break;
    case JS_VALUE_TYPE:
      JSValue::cast(this)->JSValueVerify();
      break;
    case JS_DATE_TYPE:
      JSDate::cast(this)->JSDateVerify();
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
    case JS_SET_TYPE:
      JSSet::cast(this)->JSSetVerify();
      break;
    case JS_MAP_TYPE:
      JSMap::cast(this)->JSMapVerify();
      break;
    case JS_WEAK_MAP_TYPE:
      JSWeakMap::cast(this)->JSWeakMapVerify();
      break;
    case JS_REGEXP_TYPE:
      JSRegExp::cast(this)->JSRegExpVerify();
      break;
    case FILLER_TYPE:
      break;
    case JS_PROXY_TYPE:
      JSProxy::cast(this)->JSProxyVerify();
      break;
    case JS_FUNCTION_PROXY_TYPE:
      JSFunctionProxy::cast(this)->JSFunctionProxyVerify();
      break;
    case FOREIGN_TYPE:
      Foreign::cast(this)->ForeignVerify();
      break;
    case SHARED_FUNCTION_INFO_TYPE:
      SharedFunctionInfo::cast(this)->SharedFunctionInfoVerify();
      break;
    case JS_MESSAGE_OBJECT_TYPE:
      JSMessageObject::cast(this)->JSMessageObjectVerify();
      break;
    case JS_ARRAY_BUFFER_TYPE:
      JSArrayBuffer::cast(this)->JSArrayBufferVerify();
      break;
    case JS_TYPED_ARRAY_TYPE:
      JSTypedArray::cast(this)->JSTypedArrayVerify();
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
  CHECK(p->IsHeapObject());
  CHECK(HEAP->Contains(HeapObject::cast(p)));
}


void Symbol::SymbolVerify() {
  CHECK(IsSymbol());
  CHECK(HasHashCode());
  CHECK_GT(Hash(), 0);
  CHECK(name()->IsUndefined() || name()->IsString());
}


void HeapNumber::HeapNumberVerify() {
  CHECK(IsHeapNumber());
}


void ByteArray::ByteArrayVerify() {
  CHECK(IsByteArray());
}


void FreeSpace::FreeSpaceVerify() {
  CHECK(IsFreeSpace());
}


void ExternalPixelArray::ExternalPixelArrayVerify() {
  CHECK(IsExternalPixelArray());
}


void ExternalByteArray::ExternalByteArrayVerify() {
  CHECK(IsExternalByteArray());
}


void ExternalUnsignedByteArray::ExternalUnsignedByteArrayVerify() {
  CHECK(IsExternalUnsignedByteArray());
}


void ExternalShortArray::ExternalShortArrayVerify() {
  CHECK(IsExternalShortArray());
}


void ExternalUnsignedShortArray::ExternalUnsignedShortArrayVerify() {
  CHECK(IsExternalUnsignedShortArray());
}


void ExternalIntArray::ExternalIntArrayVerify() {
  CHECK(IsExternalIntArray());
}


void ExternalUnsignedIntArray::ExternalUnsignedIntArrayVerify() {
  CHECK(IsExternalUnsignedIntArray());
}


void ExternalFloatArray::ExternalFloatArrayVerify() {
  CHECK(IsExternalFloatArray());
}


void ExternalDoubleArray::ExternalDoubleArrayVerify() {
  CHECK(IsExternalDoubleArray());
}


void JSObject::JSObjectVerify() {
  VerifyHeapPointer(properties());
  VerifyHeapPointer(elements());

  if (GetElementsKind() == NON_STRICT_ARGUMENTS_ELEMENTS) {
    CHECK(this->elements()->IsFixedArray());
    CHECK_GE(this->elements()->length(), 2);
  }

  if (HasFastProperties()) {
    CHECK_EQ(map()->unused_property_fields(),
             (map()->inobject_properties() + properties()->length() -
              map()->NextFreePropertyIndex()));
  }
  CHECK_EQ((map()->has_fast_smi_or_object_elements() ||
             (elements() == GetHeap()->empty_fixed_array())),
            (elements()->map() == GetHeap()->fixed_array_map() ||
             elements()->map() == GetHeap()->fixed_cow_array_map()));
  CHECK(map()->has_fast_object_elements() == HasFastObjectElements());
}


void Map::MapVerify() {
  CHECK(!HEAP->InNewSpace(this));
  CHECK(FIRST_TYPE <= instance_type() && instance_type() <= LAST_TYPE);
  CHECK(instance_size() == kVariableSizeSentinel ||
         (kPointerSize <= instance_size() &&
          instance_size() < HEAP->Capacity()));
  VerifyHeapPointer(prototype());
  VerifyHeapPointer(instance_descriptors());
  SLOW_ASSERT(instance_descriptors()->IsSortedNoDuplicates());
  if (HasTransitionArray()) {
    SLOW_ASSERT(transitions()->IsSortedNoDuplicates());
    SLOW_ASSERT(transitions()->IsConsistentWithBackPointers(this));
  }
  ASSERT(!is_observed() || instance_type() < FIRST_JS_OBJECT_TYPE ||
         instance_type() > LAST_JS_OBJECT_TYPE ||
         has_slow_elements_kind() || has_external_array_elements());
}


void Map::SharedMapVerify() {
  MapVerify();
  CHECK(is_shared());
  CHECK(instance_descriptors()->IsEmpty());
  CHECK_EQ(0, pre_allocated_property_fields());
  CHECK_EQ(0, unused_property_fields());
  CHECK_EQ(StaticVisitorBase::GetVisitorId(instance_type(), instance_size()),
      visitor_id());
}


void Map::VerifyOmittedPrototypeChecks() {
  if (!FLAG_omit_prototype_checks_for_leaf_maps) return;
  if (HasTransitionArray() || is_dictionary_map()) {
    CHECK_EQ(0, dependent_code()->number_of_entries(
        DependentCode::kPrototypeCheckGroup));
  }
}


void CodeCache::CodeCacheVerify() {
  VerifyHeapPointer(default_cache());
  VerifyHeapPointer(normal_type_cache());
  CHECK(default_cache()->IsFixedArray());
  CHECK(normal_type_cache()->IsUndefined()
         || normal_type_cache()->IsCodeCacheHashTable());
}


void PolymorphicCodeCache::PolymorphicCodeCacheVerify() {
  VerifyHeapPointer(cache());
  CHECK(cache()->IsUndefined() || cache()->IsPolymorphicCodeCacheHashTable());
}


void TypeFeedbackInfo::TypeFeedbackInfoVerify() {
  VerifyObjectField(kStorage1Offset);
  VerifyObjectField(kStorage2Offset);
  VerifyHeapPointer(type_feedback_cells());
}


void AliasedArgumentsEntry::AliasedArgumentsEntryVerify() {
  VerifySmiField(kAliasedContextSlot);
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


void FixedDoubleArray::FixedDoubleArrayVerify() {
  for (int i = 0; i < length(); i++) {
    if (!is_the_hole(i)) {
      double value = get_scalar(i);
      CHECK(!std::isnan(value) ||
             (BitCast<uint64_t>(value) ==
              BitCast<uint64_t>(canonical_not_the_hole_nan_as_double())) ||
             ((BitCast<uint64_t>(value) & Double::kSignMask) != 0));
    }
  }
}


void JSGeneratorObject::JSGeneratorObjectVerify() {
  // In an expression like "new g()", there can be a point where a generator
  // object is allocated but its fields are all undefined, as it hasn't yet been
  // initialized by the generator.  Hence these weak checks.
  VerifyObjectField(kFunctionOffset);
  VerifyObjectField(kContextOffset);
  VerifyObjectField(kReceiverOffset);
  VerifyObjectField(kOperandStackOffset);
  VerifyObjectField(kContinuationOffset);
  VerifyObjectField(kStackHandlerIndexOffset);
}


void JSModule::JSModuleVerify() {
  VerifyObjectField(kContextOffset);
  VerifyObjectField(kScopeInfoOffset);
  CHECK(context()->IsUndefined() ||
        Context::cast(context())->IsModuleContext());
}


void JSValue::JSValueVerify() {
  Object* v = value();
  if (v->IsHeapObject()) {
    VerifyHeapPointer(v);
  }
}


void JSDate::JSDateVerify() {
  if (value()->IsHeapObject()) {
    VerifyHeapPointer(value());
  }
  CHECK(value()->IsUndefined() || value()->IsSmi() || value()->IsHeapNumber());
  CHECK(year()->IsUndefined() || year()->IsSmi() || year()->IsNaN());
  CHECK(month()->IsUndefined() || month()->IsSmi() || month()->IsNaN());
  CHECK(day()->IsUndefined() || day()->IsSmi() || day()->IsNaN());
  CHECK(weekday()->IsUndefined() || weekday()->IsSmi() || weekday()->IsNaN());
  CHECK(hour()->IsUndefined() || hour()->IsSmi() || hour()->IsNaN());
  CHECK(min()->IsUndefined() || min()->IsSmi() || min()->IsNaN());
  CHECK(sec()->IsUndefined() || sec()->IsSmi() || sec()->IsNaN());
  CHECK(cache_stamp()->IsUndefined() ||
        cache_stamp()->IsSmi() ||
        cache_stamp()->IsNaN());

  if (month()->IsSmi()) {
    int month = Smi::cast(this->month())->value();
    CHECK(0 <= month && month <= 11);
  }
  if (day()->IsSmi()) {
    int day = Smi::cast(this->day())->value();
    CHECK(1 <= day && day <= 31);
  }
  if (hour()->IsSmi()) {
    int hour = Smi::cast(this->hour())->value();
    CHECK(0 <= hour && hour <= 23);
  }
  if (min()->IsSmi()) {
    int min = Smi::cast(this->min())->value();
    CHECK(0 <= min && min <= 59);
  }
  if (sec()->IsSmi()) {
    int sec = Smi::cast(this->sec())->value();
    CHECK(0 <= sec && sec <= 59);
  }
  if (weekday()->IsSmi()) {
    int weekday = Smi::cast(this->weekday())->value();
    CHECK(0 <= weekday && weekday <= 6);
  }
  if (cache_stamp()->IsSmi()) {
    CHECK(Smi::cast(cache_stamp())->value() <=
          Smi::cast(Isolate::Current()->date_cache()->stamp())->value());
  }
}


void JSMessageObject::JSMessageObjectVerify() {
  CHECK(IsJSMessageObject());
  CHECK(type()->IsString());
  CHECK(arguments()->IsJSArray());
  VerifyObjectField(kStartPositionOffset);
  VerifyObjectField(kEndPositionOffset);
  VerifyObjectField(kArgumentsOffset);
  VerifyObjectField(kScriptOffset);
  VerifyObjectField(kStackTraceOffset);
  VerifyObjectField(kStackFramesOffset);
}


void String::StringVerify() {
  CHECK(IsString());
  CHECK(length() >= 0 && length() <= Smi::kMaxValue);
  if (IsInternalizedString()) {
    CHECK(!HEAP->InNewSpace(this));
  }
  if (IsConsString()) {
    ConsString::cast(this)->ConsStringVerify();
  } else if (IsSlicedString()) {
    SlicedString::cast(this)->SlicedStringVerify();
  }
}


void ConsString::ConsStringVerify() {
  CHECK(this->first()->IsString());
  CHECK(this->second() == GetHeap()->empty_string() ||
        this->second()->IsString());
  CHECK(this->length() >= ConsString::kMinLength);
  if (this->IsFlat()) {
    // A flat cons can only be created by String::SlowTryFlatten.
    // Afterwards, the first part may be externalized.
    CHECK(this->first()->IsSeqString() || this->first()->IsExternalString());
  }
}


void SlicedString::SlicedStringVerify() {
  CHECK(!this->parent()->IsConsString());
  CHECK(!this->parent()->IsSlicedString());
  CHECK(this->length() >= SlicedString::kMinLength);
}


void JSFunction::JSFunctionVerify() {
  CHECK(IsJSFunction());
  VerifyObjectField(kPrototypeOrInitialMapOffset);
  VerifyObjectField(kNextFunctionLinkOffset);
  CHECK(code()->IsCode());
  CHECK(next_function_link() == NULL ||
        next_function_link()->IsUndefined() ||
        next_function_link()->IsJSFunction());
}


void SharedFunctionInfo::SharedFunctionInfoVerify() {
  CHECK(IsSharedFunctionInfo());
  VerifyObjectField(kNameOffset);
  VerifyObjectField(kCodeOffset);
  VerifyObjectField(kOptimizedCodeMapOffset);
  VerifyObjectField(kScopeInfoOffset);
  VerifyObjectField(kInstanceClassNameOffset);
  VerifyObjectField(kFunctionDataOffset);
  VerifyObjectField(kScriptOffset);
  VerifyObjectField(kDebugInfoOffset);
}


void JSGlobalProxy::JSGlobalProxyVerify() {
  CHECK(IsJSGlobalProxy());
  JSObjectVerify();
  VerifyObjectField(JSGlobalProxy::kNativeContextOffset);
  // Make sure that this object has no properties, elements.
  CHECK_EQ(0, properties()->length());
  CHECK(HasFastObjectElements());
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
    CHECK(number == HEAP->nan_value());
  } else {
    CHECK(number->IsSmi());
    int value = Smi::cast(number)->value();
    // Hidden oddballs have negative smis.
    const int kLeastHiddenOddballNumber = -4;
    CHECK_LE(value, 1);
    CHECK(value >= kLeastHiddenOddballNumber);
  }
}


void JSGlobalPropertyCell::JSGlobalPropertyCellVerify() {
  CHECK(IsJSGlobalPropertyCell());
  VerifyObjectField(kValueOffset);
}


void Code::CodeVerify() {
  CHECK(IsAligned(reinterpret_cast<intptr_t>(instruction_start()),
                  kCodeAlignment));
  relocation_info()->Verify();
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


void Code::VerifyEmbeddedMapsDependency() {
  int mode_mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT &&
      it.rinfo()->target_object()->IsMap()) {
      Map* map = Map::cast(it.rinfo()->target_object());
      if (map->CanTransition()) {
        CHECK(map->dependent_code()->Contains(
            DependentCode::kWeaklyEmbeddedGroup, this));
      }
    }
  }
}


void JSArray::JSArrayVerify() {
  JSObjectVerify();
  CHECK(length()->IsNumber() || length()->IsUndefined());
  CHECK(elements()->IsUndefined() ||
         elements()->IsFixedArray() ||
         elements()->IsFixedDoubleArray());
}


void JSSet::JSSetVerify() {
  CHECK(IsJSSet());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsHashTable() || table()->IsUndefined());
}


void JSMap::JSMapVerify() {
  CHECK(IsJSMap());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsHashTable() || table()->IsUndefined());
}


void JSWeakMap::JSWeakMapVerify() {
  CHECK(IsJSWeakMap());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsHashTable() || table()->IsUndefined());
}


void JSRegExp::JSRegExpVerify() {
  JSObjectVerify();
  CHECK(data()->IsUndefined() || data()->IsFixedArray());
  switch (TypeTag()) {
    case JSRegExp::ATOM: {
      FixedArray* arr = FixedArray::cast(data());
      CHECK(arr->get(JSRegExp::kAtomPatternIndex)->IsString());
      break;
    }
    case JSRegExp::IRREGEXP: {
      bool is_native = RegExpImpl::UsesNativeRegExp();

      FixedArray* arr = FixedArray::cast(data());
      Object* ascii_data = arr->get(JSRegExp::kIrregexpASCIICodeIndex);
      // Smi : Not compiled yet (-1) or code prepared for flushing.
      // JSObject: Compilation error.
      // Code/ByteArray: Compiled code.
      CHECK(ascii_data->IsSmi() ||
             (is_native ? ascii_data->IsCode() : ascii_data->IsByteArray()));
      Object* uc16_data = arr->get(JSRegExp::kIrregexpUC16CodeIndex);
      CHECK(uc16_data->IsSmi() ||
             (is_native ? uc16_data->IsCode() : uc16_data->IsByteArray()));

      Object* ascii_saved = arr->get(JSRegExp::kIrregexpASCIICodeSavedIndex);
      CHECK(ascii_saved->IsSmi() || ascii_saved->IsString() ||
             ascii_saved->IsCode());
      Object* uc16_saved = arr->get(JSRegExp::kIrregexpUC16CodeSavedIndex);
      CHECK(uc16_saved->IsSmi() || uc16_saved->IsString() ||
             uc16_saved->IsCode());

      CHECK(arr->get(JSRegExp::kIrregexpCaptureCountIndex)->IsSmi());
      CHECK(arr->get(JSRegExp::kIrregexpMaxRegisterCountIndex)->IsSmi());
      break;
    }
    default:
      CHECK_EQ(JSRegExp::NOT_COMPILED, TypeTag());
      CHECK(data()->IsUndefined());
      break;
  }
}


void JSProxy::JSProxyVerify() {
  CHECK(IsJSProxy());
  VerifyPointer(handler());
  CHECK(hash()->IsSmi() || hash()->IsUndefined());
}


void JSFunctionProxy::JSFunctionProxyVerify() {
  CHECK(IsJSFunctionProxy());
  JSProxyVerify();
  VerifyPointer(call_trap());
  VerifyPointer(construct_trap());
}

void JSArrayBuffer::JSArrayBufferVerify() {
  CHECK(IsJSArrayBuffer());
  JSObjectVerify();
  VerifyPointer(byte_length());
  CHECK(byte_length()->IsSmi() || byte_length()->IsHeapNumber()
        || byte_length()->IsUndefined());
}


void JSTypedArray::JSTypedArrayVerify() {
  CHECK(IsJSTypedArray());
  JSObjectVerify();
  VerifyPointer(buffer());
  CHECK(buffer()->IsJSArrayBuffer() || buffer()->IsUndefined());

  VerifyPointer(byte_offset());
  CHECK(byte_offset()->IsSmi() || byte_offset()->IsHeapNumber()
        || byte_offset()->IsUndefined());

  VerifyPointer(byte_length());
  CHECK(byte_length()->IsSmi() || byte_length()->IsHeapNumber()
        || byte_length()->IsUndefined());

  VerifyPointer(length());
  CHECK(length()->IsSmi() || length()->IsHeapNumber()
        || length()->IsUndefined());

  VerifyPointer(elements());
}


void Foreign::ForeignVerify() {
  CHECK(IsForeign());
}


void AccessorInfo::AccessorInfoVerify() {
  VerifyPointer(name());
  VerifyPointer(flag());
  VerifyPointer(expected_receiver_type());
}


void ExecutableAccessorInfo::ExecutableAccessorInfoVerify() {
  CHECK(IsExecutableAccessorInfo());
  AccessorInfoVerify();
  VerifyPointer(getter());
  VerifyPointer(setter());
  VerifyPointer(data());
}


void DeclaredAccessorDescriptor::DeclaredAccessorDescriptorVerify() {
  CHECK(IsDeclaredAccessorDescriptor());
  VerifyPointer(serialized_data());
}


void DeclaredAccessorInfo::DeclaredAccessorInfoVerify() {
  CHECK(IsDeclaredAccessorInfo());
  AccessorInfoVerify();
  VerifyPointer(descriptor());
}


void AccessorPair::AccessorPairVerify() {
  CHECK(IsAccessorPair());
  VerifyPointer(getter());
  VerifyPointer(setter());
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


void AllocationSiteInfo::AllocationSiteInfoVerify() {
  CHECK(IsAllocationSiteInfo());
  VerifyHeapPointer(payload());
  CHECK(payload()->IsObject());
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


void JSFunctionResultCache::JSFunctionResultCacheVerify() {
  JSFunction::cast(get(kFactoryIndex))->Verify();

  int size = Smi::cast(get(kCacheSizeIndex))->value();
  CHECK(kEntriesIndex <= size);
  CHECK(size <= length());
  CHECK_EQ(0, size % kEntrySize);

  int finger = Smi::cast(get(kFingerIndex))->value();
  CHECK(kEntriesIndex <= finger);
  CHECK((finger < size) || (finger == kEntriesIndex && finger == size));
  CHECK_EQ(0, finger % kEntrySize);

  if (FLAG_enable_slow_asserts) {
    for (int i = kEntriesIndex; i < size; i++) {
      CHECK(!get(i)->IsTheHole());
      get(i)->Verify();
    }
    for (int i = size; i < length(); i++) {
      CHECK(get(i)->IsTheHole());
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
        CHECK(e->IsUndefined());
      }
    }
  }
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
#endif  // VERIFY_HEAP

#ifdef DEBUG

void JSObject::IncrementSpillStatistics(SpillInformation* info) {
  info->number_of_objects_++;
  // Named properties
  if (HasFastProperties()) {
    info->number_of_objects_with_fast_properties_++;
    info->number_of_fast_used_fields_   += map()->NextFreePropertyIndex();
    info->number_of_fast_unused_fields_ += map()->unused_property_fields();
  } else {
    NameDictionary* dict = property_dictionary();
    info->number_of_slow_used_properties_ += dict->NumberOfElements();
    info->number_of_slow_unused_properties_ +=
        dict->Capacity() - dict->NumberOfElements();
  }
  // Indexed properties
  switch (GetElementsKind()) {
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_SMI_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case FAST_ELEMENTS: {
      info->number_of_objects_with_fast_elements_++;
      int holes = 0;
      FixedArray* e = FixedArray::cast(elements());
      int len = e->length();
      Heap* heap = HEAP;
      for (int i = 0; i < len; i++) {
        if (e->get(i) == heap->the_hole_value()) holes++;
      }
      info->number_of_fast_used_elements_   += len - holes;
      info->number_of_fast_unused_elements_ += holes;
      break;
    }
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS:
    case EXTERNAL_PIXEL_ELEMENTS: {
      info->number_of_objects_with_fast_elements_++;
      ExternalPixelArray* e = ExternalPixelArray::cast(elements());
      info->number_of_fast_used_elements_ += e->length();
      break;
    }
    case DICTIONARY_ELEMENTS: {
      SeededNumberDictionary* dict = element_dictionary();
      info->number_of_slow_used_elements_ += dict->NumberOfElements();
      info->number_of_slow_unused_elements_ +=
          dict->Capacity() - dict->NumberOfElements();
      break;
    }
    case NON_STRICT_ARGUMENTS_ELEMENTS:
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


bool DescriptorArray::IsSortedNoDuplicates(int valid_entries) {
  if (valid_entries == -1) valid_entries = number_of_descriptors();
  Name* current_key = NULL;
  uint32_t current = 0;
  for (int i = 0; i < number_of_descriptors(); i++) {
    Name* key = GetSortedKey(i);
    if (key == current_key) {
      PrintDescriptors();
      return false;
    }
    current_key = key;
    uint32_t hash = GetSortedKey(i)->Hash();
    if (hash < current) {
      PrintDescriptors();
      return false;
    }
    current = hash;
  }
  return true;
}


bool TransitionArray::IsSortedNoDuplicates(int valid_entries) {
  ASSERT(valid_entries == -1);
  Name* current_key = NULL;
  uint32_t current = 0;
  for (int i = 0; i < number_of_transitions(); i++) {
    Name* key = GetSortedKey(i);
    if (key == current_key) {
      PrintTransitions();
      return false;
    }
    current_key = key;
    uint32_t hash = GetSortedKey(i)->Hash();
    if (hash < current) {
      PrintTransitions();
      return false;
    }
    current = hash;
  }
  return true;
}


static bool CheckOneBackPointer(Map* current_map, Object* target) {
  return !target->IsMap() || Map::cast(target)->GetBackPointer() == current_map;
}


bool TransitionArray::IsConsistentWithBackPointers(Map* current_map) {
  if (HasElementsTransition() &&
      !CheckOneBackPointer(current_map, elements_transition())) {
    return false;
  }
  for (int i = 0; i < number_of_transitions(); ++i) {
    if (!CheckOneBackPointer(current_map, GetTarget(i))) return false;
  }
  return true;
}


#endif  // DEBUG

} }  // namespace v8::internal

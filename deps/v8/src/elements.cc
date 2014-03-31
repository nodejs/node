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

#include "arguments.h"
#include "objects.h"
#include "elements.h"
#include "utils.h"
#include "v8conversions.h"

// Each concrete ElementsAccessor can handle exactly one ElementsKind,
// several abstract ElementsAccessor classes are used to allow sharing
// common code.
//
// Inheritance hierarchy:
// - ElementsAccessorBase                        (abstract)
//   - FastElementsAccessor                      (abstract)
//     - FastSmiOrObjectElementsAccessor
//       - FastPackedSmiElementsAccessor
//       - FastHoleySmiElementsAccessor
//       - FastPackedObjectElementsAccessor
//       - FastHoleyObjectElementsAccessor
//     - FastDoubleElementsAccessor
//       - FastPackedDoubleElementsAccessor
//       - FastHoleyDoubleElementsAccessor
//   - TypedElementsAccessor: template, with instantiations:
//     - ExternalInt8ElementsAccessor
//     - ExternalUint8ElementsAccessor
//     - ExternalInt16ElementsAccessor
//     - ExternalUint16ElementsAccessor
//     - ExternalInt32ElementsAccessor
//     - ExternalUint32ElementsAccessor
//     - ExternalFloat32ElementsAccessor
//     - ExternalFloat64ElementsAccessor
//     - ExternalUint8ClampedElementsAccessor
//     - FixedUint8ElementsAccessor
//     - FixedInt8ElementsAccessor
//     - FixedUint16ElementsAccessor
//     - FixedInt16ElementsAccessor
//     - FixedUint32ElementsAccessor
//     - FixedInt32ElementsAccessor
//     - FixedFloat32ElementsAccessor
//     - FixedFloat64ElementsAccessor
//     - FixedUint8ClampedElementsAccessor
//   - DictionaryElementsAccessor
//   - SloppyArgumentsElementsAccessor


namespace v8 {
namespace internal {


static const int kPackedSizeNotKnown = -1;


// First argument in list is the accessor class, the second argument is the
// accessor ElementsKind, and the third is the backing store class.  Use the
// fast element handler for smi-only arrays.  The implementation is currently
// identical.  Note that the order must match that of the ElementsKind enum for
// the |accessor_array[]| below to work.
#define ELEMENTS_LIST(V)                                                \
  V(FastPackedSmiElementsAccessor, FAST_SMI_ELEMENTS, FixedArray)       \
  V(FastHoleySmiElementsAccessor, FAST_HOLEY_SMI_ELEMENTS,              \
    FixedArray)                                                         \
  V(FastPackedObjectElementsAccessor, FAST_ELEMENTS, FixedArray)        \
  V(FastHoleyObjectElementsAccessor, FAST_HOLEY_ELEMENTS, FixedArray)   \
  V(FastPackedDoubleElementsAccessor, FAST_DOUBLE_ELEMENTS,             \
    FixedDoubleArray)                                                   \
  V(FastHoleyDoubleElementsAccessor, FAST_HOLEY_DOUBLE_ELEMENTS,        \
    FixedDoubleArray)                                                   \
  V(DictionaryElementsAccessor, DICTIONARY_ELEMENTS,                    \
    SeededNumberDictionary)                                             \
  V(SloppyArgumentsElementsAccessor, SLOPPY_ARGUMENTS_ELEMENTS,         \
    FixedArray)                                                         \
  V(ExternalInt8ElementsAccessor, EXTERNAL_INT8_ELEMENTS,               \
    ExternalInt8Array)                                                  \
  V(ExternalUint8ElementsAccessor,                                      \
    EXTERNAL_UINT8_ELEMENTS, ExternalUint8Array)                        \
  V(ExternalInt16ElementsAccessor, EXTERNAL_INT16_ELEMENTS,             \
    ExternalInt16Array)                                                 \
  V(ExternalUint16ElementsAccessor,                                     \
    EXTERNAL_UINT16_ELEMENTS, ExternalUint16Array)                      \
  V(ExternalInt32ElementsAccessor, EXTERNAL_INT32_ELEMENTS,             \
    ExternalInt32Array)                                                 \
  V(ExternalUint32ElementsAccessor,                                     \
    EXTERNAL_UINT32_ELEMENTS, ExternalUint32Array)                      \
  V(ExternalFloat32ElementsAccessor,                                    \
    EXTERNAL_FLOAT32_ELEMENTS, ExternalFloat32Array)                    \
  V(ExternalFloat64ElementsAccessor,                                    \
    EXTERNAL_FLOAT64_ELEMENTS, ExternalFloat64Array)                    \
  V(ExternalUint8ClampedElementsAccessor,                               \
    EXTERNAL_UINT8_CLAMPED_ELEMENTS,                                    \
    ExternalUint8ClampedArray)                                          \
  V(FixedUint8ElementsAccessor, UINT8_ELEMENTS, FixedUint8Array)        \
  V(FixedInt8ElementsAccessor, INT8_ELEMENTS, FixedInt8Array)           \
  V(FixedUint16ElementsAccessor, UINT16_ELEMENTS, FixedUint16Array)     \
  V(FixedInt16ElementsAccessor, INT16_ELEMENTS, FixedInt16Array)        \
  V(FixedUint32ElementsAccessor, UINT32_ELEMENTS, FixedUint32Array)     \
  V(FixedInt32ElementsAccessor, INT32_ELEMENTS, FixedInt32Array)        \
  V(FixedFloat32ElementsAccessor, FLOAT32_ELEMENTS, FixedFloat32Array)  \
  V(FixedFloat64ElementsAccessor, FLOAT64_ELEMENTS, FixedFloat64Array)  \
  V(FixedUint8ClampedElementsAccessor, UINT8_CLAMPED_ELEMENTS,          \
    FixedUint8ClampedArray)


template<ElementsKind Kind> class ElementsKindTraits {
 public:
  typedef FixedArrayBase BackingStore;
};

#define ELEMENTS_TRAITS(Class, KindParam, Store)               \
template<> class ElementsKindTraits<KindParam> {               \
  public:                                                      \
  static const ElementsKind Kind = KindParam;                  \
  typedef Store BackingStore;                                  \
};
ELEMENTS_LIST(ELEMENTS_TRAITS)
#undef ELEMENTS_TRAITS


ElementsAccessor** ElementsAccessor::elements_accessors_;


static bool HasKey(FixedArray* array, Object* key) {
  int len0 = array->length();
  for (int i = 0; i < len0; i++) {
    Object* element = array->get(i);
    if (element->IsSmi() && element == key) return true;
    if (element->IsString() &&
        key->IsString() && String::cast(element)->Equals(String::cast(key))) {
      return true;
    }
  }
  return false;
}


static Handle<Object> ThrowArrayLengthRangeError(Isolate* isolate) {
  isolate->Throw(
      *isolate->factory()->NewRangeError("invalid_array_length",
                                         HandleVector<Object>(NULL, 0)));
  return Handle<Object>();
}


static void CopyObjectToObjectElements(Handle<FixedArrayBase> from_base,
                                       ElementsKind from_kind,
                                       uint32_t from_start,
                                       Handle<FixedArrayBase> to_base,
                                       ElementsKind to_kind,
                                       uint32_t to_start,
                                       int raw_copy_size) {
  ASSERT(to_base->map() !=
      from_base->GetIsolate()->heap()->fixed_cow_array_map());
  DisallowHeapAllocation no_allocation;
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = Min(from_base->length() - from_start,
                    to_base->length() - to_start);
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      int start = to_start + copy_size;
      int length = to_base->length() - start;
      if (length > 0) {
        Heap* heap = from_base->GetHeap();
        MemsetPointer(Handle<FixedArray>::cast(to_base)->data_start() + start,
                      heap->the_hole_value(), length);
      }
    }
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  Handle<FixedArray> from = Handle<FixedArray>::cast(from_base);
  Handle<FixedArray> to = Handle<FixedArray>::cast(to_base);
  ASSERT(IsFastSmiOrObjectElementsKind(from_kind));
  ASSERT(IsFastSmiOrObjectElementsKind(to_kind));
  Address to_address = to->address() + FixedArray::kHeaderSize;
  Address from_address = from->address() + FixedArray::kHeaderSize;
  CopyWords(reinterpret_cast<Object**>(to_address) + to_start,
            reinterpret_cast<Object**>(from_address) + from_start,
            static_cast<size_t>(copy_size));
  if (IsFastObjectElementsKind(from_kind) &&
      IsFastObjectElementsKind(to_kind)) {
    Heap* heap = from->GetHeap();
    if (!heap->InNewSpace(*to)) {
      heap->RecordWrites(to->address(),
                         to->OffsetOfElementAt(to_start),
                         copy_size);
    }
    heap->incremental_marking()->RecordWrites(*to);
  }
}


static void CopyDictionaryToObjectElements(Handle<FixedArrayBase> from_base,
                                           uint32_t from_start,
                                           Handle<FixedArrayBase> to_base,
                                           ElementsKind to_kind,
                                           uint32_t to_start,
                                           int raw_copy_size) {
  Handle<SeededNumberDictionary> from =
      Handle<SeededNumberDictionary>::cast(from_base);
  DisallowHeapAllocation no_allocation;
  int copy_size = raw_copy_size;
  Heap* heap = from->GetHeap();
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from->max_number_key() + 1 - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      int start = to_start + copy_size;
      int length = to_base->length() - start;
      if (length > 0) {
        Heap* heap = from->GetHeap();
        MemsetPointer(Handle<FixedArray>::cast(to_base)->data_start() + start,
                      heap->the_hole_value(), length);
      }
    }
  }
  ASSERT(*to_base != *from_base);
  ASSERT(IsFastSmiOrObjectElementsKind(to_kind));
  if (copy_size == 0) return;
  Handle<FixedArray> to = Handle<FixedArray>::cast(to_base);
  uint32_t to_length = to->length();
  if (to_start + copy_size > to_length) {
    copy_size = to_length - to_start;
  }
  for (int i = 0; i < copy_size; i++) {
    int entry = from->FindEntry(i + from_start);
    if (entry != SeededNumberDictionary::kNotFound) {
      Object* value = from->ValueAt(entry);
      ASSERT(!value->IsTheHole());
      to->set(i + to_start, value, SKIP_WRITE_BARRIER);
    } else {
      to->set_the_hole(i + to_start);
    }
  }
  if (IsFastObjectElementsKind(to_kind)) {
    if (!heap->InNewSpace(*to)) {
      heap->RecordWrites(to->address(),
                         to->OffsetOfElementAt(to_start),
                         copy_size);
    }
    heap->incremental_marking()->RecordWrites(*to);
  }
}


static void CopyDoubleToObjectElements(Handle<FixedArrayBase> from_base,
                                       uint32_t from_start,
                                       Handle<FixedArrayBase> to_base,
                                       ElementsKind to_kind,
                                       uint32_t to_start,
                                       int raw_copy_size) {
  ASSERT(IsFastSmiOrObjectElementsKind(to_kind));
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = Min(from_base->length() - from_start,
                    to_base->length() - to_start);
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      // Also initialize the area that will be copied over since HeapNumber
      // allocation below can cause an incremental marking step, requiring all
      // existing heap objects to be propertly initialized.
      int start = to_start;
      int length = to_base->length() - start;
      if (length > 0) {
        Heap* heap = from_base->GetHeap();
        MemsetPointer(Handle<FixedArray>::cast(to_base)->data_start() + start,
                      heap->the_hole_value(), length);
      }
    }
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  Handle<FixedDoubleArray> from = Handle<FixedDoubleArray>::cast(from_base);
  Handle<FixedArray> to = Handle<FixedArray>::cast(to_base);
  for (int i = 0; i < copy_size; ++i) {
    HandleScope scope(from_base->GetIsolate());
    if (IsFastSmiElementsKind(to_kind)) {
      UNIMPLEMENTED();
    } else {
      ASSERT(IsFastObjectElementsKind(to_kind));
      Handle<Object> value = from->get_as_handle(i + from_start);
      to->set(i + to_start, *value, UPDATE_WRITE_BARRIER);
    }
  }
}


static void CopyDoubleToDoubleElements(Handle<FixedArrayBase> from_base,
                                       uint32_t from_start,
                                       Handle<FixedArrayBase> to_base,
                                       uint32_t to_start,
                                       int raw_copy_size) {
  DisallowHeapAllocation no_allocation;
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = Min(from_base->length() - from_start,
                    to_base->length() - to_start);
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to_base->length(); ++i) {
        Handle<FixedDoubleArray>::cast(to_base)->set_the_hole(i);
      }
    }
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  Handle<FixedDoubleArray> from = Handle<FixedDoubleArray>::cast(from_base);
  Handle<FixedDoubleArray> to = Handle<FixedDoubleArray>::cast(to_base);
  Address to_address = to->address() + FixedDoubleArray::kHeaderSize;
  Address from_address = from->address() + FixedDoubleArray::kHeaderSize;
  to_address += kDoubleSize * to_start;
  from_address += kDoubleSize * from_start;
  int words_per_double = (kDoubleSize / kPointerSize);
  CopyWords(reinterpret_cast<Object**>(to_address),
            reinterpret_cast<Object**>(from_address),
            static_cast<size_t>(words_per_double * copy_size));
}


static void CopySmiToDoubleElements(Handle<FixedArrayBase> from_base,
                                    uint32_t from_start,
                                    Handle<FixedArrayBase> to_base,
                                    uint32_t to_start,
                                    int raw_copy_size) {
  DisallowHeapAllocation no_allocation;
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from_base->length() - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to_base->length(); ++i) {
        Handle<FixedDoubleArray>::cast(to_base)->set_the_hole(i);
      }
    }
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  Handle<FixedArray> from = Handle<FixedArray>::cast(from_base);
  Handle<FixedDoubleArray> to = Handle<FixedDoubleArray>::cast(to_base);
  Handle<Object> the_hole = from->GetIsolate()->factory()->the_hole_value();
  for (uint32_t from_end = from_start + static_cast<uint32_t>(copy_size);
       from_start < from_end; from_start++, to_start++) {
    Object* hole_or_smi = from->get(from_start);
    if (hole_or_smi == *the_hole) {
      to->set_the_hole(to_start);
    } else {
      to->set(to_start, Smi::cast(hole_or_smi)->value());
    }
  }
}


static void CopyPackedSmiToDoubleElements(Handle<FixedArrayBase> from_base,
                                          uint32_t from_start,
                                          Handle<FixedArrayBase> to_base,
                                          uint32_t to_start,
                                          int packed_size,
                                          int raw_copy_size) {
  DisallowHeapAllocation no_allocation;
  int copy_size = raw_copy_size;
  uint32_t to_end;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = packed_size - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      to_end = to_base->length();
      for (uint32_t i = to_start + copy_size; i < to_end; ++i) {
        Handle<FixedDoubleArray>::cast(to_base)->set_the_hole(i);
      }
    } else {
      to_end = to_start + static_cast<uint32_t>(copy_size);
    }
  } else {
    to_end = to_start + static_cast<uint32_t>(copy_size);
  }
  ASSERT(static_cast<int>(to_end) <= to_base->length());
  ASSERT(packed_size >= 0 && packed_size <= copy_size);
  ASSERT((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  Handle<FixedArray> from = Handle<FixedArray>::cast(from_base);
  Handle<FixedDoubleArray> to = Handle<FixedDoubleArray>::cast(to_base);
  for (uint32_t from_end = from_start + static_cast<uint32_t>(packed_size);
       from_start < from_end; from_start++, to_start++) {
    Object* smi = from->get(from_start);
    ASSERT(!smi->IsTheHole());
    to->set(to_start, Smi::cast(smi)->value());
  }
}


static void CopyObjectToDoubleElements(Handle<FixedArrayBase> from_base,
                                       uint32_t from_start,
                                       Handle<FixedArrayBase> to_base,
                                       uint32_t to_start,
                                       int raw_copy_size) {
  DisallowHeapAllocation no_allocation;
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from_base->length() - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to_base->length(); ++i) {
        Handle<FixedDoubleArray>::cast(to_base)->set_the_hole(i);
      }
    }
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  Handle<FixedArray> from = Handle<FixedArray>::cast(from_base);
  Handle<FixedDoubleArray> to = Handle<FixedDoubleArray>::cast(to_base);
  Handle<Object> the_hole = from->GetIsolate()->factory()->the_hole_value();
  for (uint32_t from_end = from_start + copy_size;
       from_start < from_end; from_start++, to_start++) {
    Object* hole_or_object = from->get(from_start);
    if (hole_or_object == *the_hole) {
      to->set_the_hole(to_start);
    } else {
      to->set(to_start, hole_or_object->Number());
    }
  }
}


static void CopyDictionaryToDoubleElements(Handle<FixedArrayBase> from_base,
                                           uint32_t from_start,
                                           Handle<FixedArrayBase> to_base,
                                           uint32_t to_start,
                                           int raw_copy_size) {
  Handle<SeededNumberDictionary> from =
      Handle<SeededNumberDictionary>::cast(from_base);
  DisallowHeapAllocation no_allocation;
  int copy_size = raw_copy_size;
  if (copy_size < 0) {
    ASSERT(copy_size == ElementsAccessor::kCopyToEnd ||
           copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from->max_number_key() + 1 - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to_base->length(); ++i) {
        Handle<FixedDoubleArray>::cast(to_base)->set_the_hole(i);
      }
    }
  }
  if (copy_size == 0) return;
  Handle<FixedDoubleArray> to = Handle<FixedDoubleArray>::cast(to_base);
  uint32_t to_length = to->length();
  if (to_start + copy_size > to_length) {
    copy_size = to_length - to_start;
  }
  for (int i = 0; i < copy_size; i++) {
    int entry = from->FindEntry(i + from_start);
    if (entry != SeededNumberDictionary::kNotFound) {
      to->set(i + to_start, from->ValueAt(entry)->Number());
    } else {
      to->set_the_hole(i + to_start);
    }
  }
}


static void TraceTopFrame(Isolate* isolate) {
  StackFrameIterator it(isolate);
  if (it.done()) {
    PrintF("unknown location (no JavaScript frames present)");
    return;
  }
  StackFrame* raw_frame = it.frame();
  if (raw_frame->is_internal()) {
    Code* apply_builtin = isolate->builtins()->builtin(
        Builtins::kFunctionApply);
    if (raw_frame->unchecked_code() == apply_builtin) {
      PrintF("apply from ");
      it.Advance();
      raw_frame = it.frame();
    }
  }
  JavaScriptFrame::PrintTop(isolate, stdout, false, true);
}


void CheckArrayAbuse(JSObject* obj, const char* op, uint32_t key,
                     bool allow_appending) {
  Object* raw_length = NULL;
  const char* elements_type = "array";
  if (obj->IsJSArray()) {
    JSArray* array = JSArray::cast(obj);
    raw_length = array->length();
  } else {
    raw_length = Smi::FromInt(obj->elements()->length());
    elements_type = "object";
  }

  if (raw_length->IsNumber()) {
    double n = raw_length->Number();
    if (FastI2D(FastD2UI(n)) == n) {
      int32_t int32_length = DoubleToInt32(n);
      uint32_t compare_length = static_cast<uint32_t>(int32_length);
      if (allow_appending) compare_length++;
      if (key >= compare_length) {
        PrintF("[OOB %s %s (%s length = %d, element accessed = %d) in ",
               elements_type, op, elements_type,
               static_cast<int>(int32_length),
               static_cast<int>(key));
        TraceTopFrame(obj->GetIsolate());
        PrintF("]\n");
      }
    } else {
      PrintF("[%s elements length not integer value in ", elements_type);
      TraceTopFrame(obj->GetIsolate());
      PrintF("]\n");
    }
  } else {
    PrintF("[%s elements length not a number in ", elements_type);
    TraceTopFrame(obj->GetIsolate());
    PrintF("]\n");
  }
}


// Base class for element handler implementations. Contains the
// the common logic for objects with different ElementsKinds.
// Subclasses must specialize method for which the element
// implementation differs from the base class implementation.
//
// This class is intended to be used in the following way:
//
//   class SomeElementsAccessor :
//       public ElementsAccessorBase<SomeElementsAccessor,
//                                   BackingStoreClass> {
//     ...
//   }
//
// This is an example of the Curiously Recurring Template Pattern (see
// http://en.wikipedia.org/wiki/Curiously_recurring_template_pattern).  We use
// CRTP to guarantee aggressive compile time optimizations (i.e.  inlining and
// specialization of SomeElementsAccessor methods).
template <typename ElementsAccessorSubclass,
          typename ElementsTraitsParam>
class ElementsAccessorBase : public ElementsAccessor {
 protected:
  explicit ElementsAccessorBase(const char* name)
      : ElementsAccessor(name) { }

  typedef ElementsTraitsParam ElementsTraits;
  typedef typename ElementsTraitsParam::BackingStore BackingStore;

  virtual ElementsKind kind() const V8_FINAL V8_OVERRIDE {
    return ElementsTraits::Kind;
  }

  static void ValidateContents(JSObject* holder, int length) {
  }

  static void ValidateImpl(JSObject* holder) {
    FixedArrayBase* fixed_array_base = holder->elements();
    // When objects are first allocated, its elements are Failures.
    if (fixed_array_base->IsFailure()) return;
    if (!fixed_array_base->IsHeapObject()) return;
    // Arrays that have been shifted in place can't be verified.
    if (fixed_array_base->IsFiller()) return;
    int length = 0;
    if (holder->IsJSArray()) {
      Object* length_obj = JSArray::cast(holder)->length();
      if (length_obj->IsSmi()) {
        length = Smi::cast(length_obj)->value();
      }
    } else {
      length = fixed_array_base->length();
    }
    ElementsAccessorSubclass::ValidateContents(holder, length);
  }

  virtual void Validate(JSObject* holder) V8_FINAL V8_OVERRIDE {
    ElementsAccessorSubclass::ValidateImpl(holder);
  }

  static bool HasElementImpl(Object* receiver,
                             JSObject* holder,
                             uint32_t key,
                             FixedArrayBase* backing_store) {
    return ElementsAccessorSubclass::GetAttributesImpl(
        receiver, holder, key, backing_store) != ABSENT;
  }

  virtual bool HasElement(Object* receiver,
                          JSObject* holder,
                          uint32_t key,
                          FixedArrayBase* backing_store) V8_FINAL V8_OVERRIDE {
    if (backing_store == NULL) {
      backing_store = holder->elements();
    }
    return ElementsAccessorSubclass::HasElementImpl(
        receiver, holder, key, backing_store);
  }

  // TODO(ishell): Temporary wrapper until handlified.
  MUST_USE_RESULT virtual Handle<Object> Get(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) V8_FINAL V8_OVERRIDE {
    CALL_HEAP_FUNCTION(holder->GetIsolate(),
                       Get(*receiver, *holder, key,
                           backing_store.is_null()
                           ? NULL : *backing_store),
                       Object);
  }

  MUST_USE_RESULT virtual MaybeObject* Get(
      Object* receiver,
      JSObject* holder,
      uint32_t key,
      FixedArrayBase* backing_store) V8_FINAL V8_OVERRIDE {
    if (backing_store == NULL) {
      backing_store = holder->elements();
    }

    if (!IsExternalArrayElementsKind(ElementsTraits::Kind) &&
        FLAG_trace_js_array_abuse) {
      CheckArrayAbuse(holder, "elements read", key);
    }

    if (IsExternalArrayElementsKind(ElementsTraits::Kind) &&
        FLAG_trace_external_array_abuse) {
      CheckArrayAbuse(holder, "external elements read", key);
    }

    return ElementsAccessorSubclass::GetImpl(
        receiver, holder, key, backing_store);
  }

  MUST_USE_RESULT static MaybeObject* GetImpl(Object* receiver,
                                              JSObject* obj,
                                              uint32_t key,
                                              FixedArrayBase* backing_store) {
    return (key < ElementsAccessorSubclass::GetCapacityImpl(backing_store))
           ? BackingStore::cast(backing_store)->get(key)
           : backing_store->GetHeap()->the_hole_value();
  }

  MUST_USE_RESULT virtual PropertyAttributes GetAttributes(
      Object* receiver,
      JSObject* holder,
      uint32_t key,
      FixedArrayBase* backing_store) V8_FINAL V8_OVERRIDE {
    if (backing_store == NULL) {
      backing_store = holder->elements();
    }
    return ElementsAccessorSubclass::GetAttributesImpl(
        receiver, holder, key, backing_store);
  }

  MUST_USE_RESULT static PropertyAttributes GetAttributesImpl(
        Object* receiver,
        JSObject* obj,
        uint32_t key,
        FixedArrayBase* backing_store) {
    if (key >= ElementsAccessorSubclass::GetCapacityImpl(backing_store)) {
      return ABSENT;
    }
    return BackingStore::cast(backing_store)->is_the_hole(key) ? ABSENT : NONE;
  }

  MUST_USE_RESULT virtual PropertyType GetType(
      Object* receiver,
      JSObject* holder,
      uint32_t key,
      FixedArrayBase* backing_store) V8_FINAL V8_OVERRIDE {
    if (backing_store == NULL) {
      backing_store = holder->elements();
    }
    return ElementsAccessorSubclass::GetTypeImpl(
        receiver, holder, key, backing_store);
  }

  MUST_USE_RESULT static PropertyType GetTypeImpl(
        Object* receiver,
        JSObject* obj,
        uint32_t key,
        FixedArrayBase* backing_store) {
    if (key >= ElementsAccessorSubclass::GetCapacityImpl(backing_store)) {
      return NONEXISTENT;
    }
    return BackingStore::cast(backing_store)->is_the_hole(key)
        ? NONEXISTENT : FIELD;
  }

  MUST_USE_RESULT virtual AccessorPair* GetAccessorPair(
      Object* receiver,
      JSObject* holder,
      uint32_t key,
      FixedArrayBase* backing_store) V8_FINAL V8_OVERRIDE {
    if (backing_store == NULL) {
      backing_store = holder->elements();
    }
    return ElementsAccessorSubclass::GetAccessorPairImpl(
        receiver, holder, key, backing_store);
  }

  MUST_USE_RESULT static AccessorPair* GetAccessorPairImpl(
        Object* receiver,
        JSObject* obj,
        uint32_t key,
        FixedArrayBase* backing_store) {
    return NULL;
  }

  MUST_USE_RESULT virtual Handle<Object> SetLength(
      Handle<JSArray> array,
      Handle<Object> length) V8_FINAL V8_OVERRIDE {
    return ElementsAccessorSubclass::SetLengthImpl(
        array, length, handle(array->elements()));
  }

  MUST_USE_RESULT static Handle<Object> SetLengthImpl(
      Handle<JSObject> obj,
      Handle<Object> length,
      Handle<FixedArrayBase> backing_store);

  virtual void SetCapacityAndLength(
      Handle<JSArray> array,
      int capacity,
      int length) V8_FINAL V8_OVERRIDE {
    ElementsAccessorSubclass::
        SetFastElementsCapacityAndLength(array, capacity, length);
  }

  static void SetFastElementsCapacityAndLength(
      Handle<JSObject> obj,
      int capacity,
      int length) {
    UNIMPLEMENTED();
  }

  MUST_USE_RESULT virtual Handle<Object> Delete(
      Handle<JSObject> obj,
      uint32_t key,
      JSReceiver::DeleteMode mode) V8_OVERRIDE = 0;

  static void CopyElementsImpl(Handle<FixedArrayBase> from,
                               uint32_t from_start,
                               Handle<FixedArrayBase> to,
                               ElementsKind from_kind,
                               uint32_t to_start,
                               int packed_size,
                               int copy_size) {
    UNREACHABLE();
  }

  virtual void CopyElements(
      Handle<JSObject> from_holder,
      uint32_t from_start,
      ElementsKind from_kind,
      Handle<FixedArrayBase> to,
      uint32_t to_start,
      int copy_size,
      Handle<FixedArrayBase> from) V8_FINAL V8_OVERRIDE {
    int packed_size = kPackedSizeNotKnown;
    if (from.is_null()) {
      from = handle(from_holder->elements());
    }

    if (!from_holder.is_null()) {
      bool is_packed = IsFastPackedElementsKind(from_kind) &&
          from_holder->IsJSArray();
      if (is_packed) {
        packed_size =
            Smi::cast(Handle<JSArray>::cast(from_holder)->length())->value();
        if (copy_size >= 0 && packed_size > copy_size) {
          packed_size = copy_size;
        }
      }
    }
    ElementsAccessorSubclass::CopyElementsImpl(
        from, from_start, to, from_kind, to_start, packed_size, copy_size);
  }

  MUST_USE_RESULT virtual MaybeObject* AddElementsToFixedArray(
      Object* receiver,
      JSObject* holder,
      FixedArray* to,
      FixedArrayBase* from) V8_FINAL V8_OVERRIDE {
    int len0 = to->length();
#ifdef ENABLE_SLOW_ASSERTS
    if (FLAG_enable_slow_asserts) {
      for (int i = 0; i < len0; i++) {
        ASSERT(!to->get(i)->IsTheHole());
      }
    }
#endif
    if (from == NULL) {
      from = holder->elements();
    }

    // Optimize if 'other' is empty.
    // We cannot optimize if 'this' is empty, as other may have holes.
    uint32_t len1 = ElementsAccessorSubclass::GetCapacityImpl(from);
    if (len1 == 0) return to;

    // Compute how many elements are not in other.
    uint32_t extra = 0;
    for (uint32_t y = 0; y < len1; y++) {
      uint32_t key = ElementsAccessorSubclass::GetKeyForIndexImpl(from, y);
      if (ElementsAccessorSubclass::HasElementImpl(
              receiver, holder, key, from)) {
        MaybeObject* maybe_value =
            ElementsAccessorSubclass::GetImpl(receiver, holder, key, from);
        Object* value;
        if (!maybe_value->To(&value)) return maybe_value;
        ASSERT(!value->IsTheHole());
        if (!HasKey(to, value)) {
          extra++;
        }
      }
    }

    if (extra == 0) return to;

    // Allocate the result
    FixedArray* result;
    MaybeObject* maybe_obj = from->GetHeap()->AllocateFixedArray(len0 + extra);
    if (!maybe_obj->To(&result)) return maybe_obj;

    // Fill in the content
    {
      DisallowHeapAllocation no_gc;
      WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
      for (int i = 0; i < len0; i++) {
        Object* e = to->get(i);
        ASSERT(e->IsString() || e->IsNumber());
        result->set(i, e, mode);
      }
    }
    // Fill in the extra values.
    uint32_t index = 0;
    for (uint32_t y = 0; y < len1; y++) {
      uint32_t key =
          ElementsAccessorSubclass::GetKeyForIndexImpl(from, y);
      if (ElementsAccessorSubclass::HasElementImpl(
              receiver, holder, key, from)) {
        MaybeObject* maybe_value =
            ElementsAccessorSubclass::GetImpl(receiver, holder, key, from);
        Object* value;
        if (!maybe_value->To(&value)) return maybe_value;
        if (!value->IsTheHole() && !HasKey(to, value)) {
          result->set(len0 + index, value);
          index++;
        }
      }
    }
    ASSERT(extra == index);
    return result;
  }

 protected:
  static uint32_t GetCapacityImpl(FixedArrayBase* backing_store) {
    return backing_store->length();
  }

  virtual uint32_t GetCapacity(FixedArrayBase* backing_store)
      V8_FINAL V8_OVERRIDE {
    return ElementsAccessorSubclass::GetCapacityImpl(backing_store);
  }

  static uint32_t GetKeyForIndexImpl(FixedArrayBase* backing_store,
                                     uint32_t index) {
    return index;
  }

  virtual uint32_t GetKeyForIndex(FixedArrayBase* backing_store,
                                  uint32_t index) V8_FINAL V8_OVERRIDE {
    return ElementsAccessorSubclass::GetKeyForIndexImpl(backing_store, index);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementsAccessorBase);
};


// Super class for all fast element arrays.
template<typename FastElementsAccessorSubclass,
         typename KindTraits,
         int ElementSize>
class FastElementsAccessor
    : public ElementsAccessorBase<FastElementsAccessorSubclass, KindTraits> {
 public:
  explicit FastElementsAccessor(const char* name)
      : ElementsAccessorBase<FastElementsAccessorSubclass,
                             KindTraits>(name) {}
 protected:
  friend class ElementsAccessorBase<FastElementsAccessorSubclass, KindTraits>;
  friend class SloppyArgumentsElementsAccessor;

  typedef typename KindTraits::BackingStore BackingStore;

  // Adjusts the length of the fast backing store or returns the new length or
  // undefined in case conversion to a slow backing store should be performed.
  static Handle<Object> SetLengthWithoutNormalize(
      Handle<FixedArrayBase> backing_store,
      Handle<JSArray> array,
      Handle<Object> length_object,
      uint32_t length) {
    Isolate* isolate = array->GetIsolate();
    uint32_t old_capacity = backing_store->length();
    Handle<Object> old_length(array->length(), isolate);
    bool same_or_smaller_size = old_length->IsSmi() &&
        static_cast<uint32_t>(Handle<Smi>::cast(old_length)->value()) >= length;
    ElementsKind kind = array->GetElementsKind();

    if (!same_or_smaller_size && IsFastElementsKind(kind) &&
        !IsFastHoleyElementsKind(kind)) {
      kind = GetHoleyElementsKind(kind);
      JSObject::TransitionElementsKind(array, kind);
    }

    // Check whether the backing store should be shrunk.
    if (length <= old_capacity) {
      if (array->HasFastSmiOrObjectElements()) {
        backing_store = JSObject::EnsureWritableFastElements(array);
      }
      if (2 * length <= old_capacity) {
        // If more than half the elements won't be used, trim the array.
        if (length == 0) {
          array->initialize_elements();
        } else {
          backing_store->set_length(length);
          Address filler_start = backing_store->address() +
              BackingStore::OffsetOfElementAt(length);
          int filler_size = (old_capacity - length) * ElementSize;
          array->GetHeap()->CreateFillerObjectAt(filler_start, filler_size);
        }
      } else {
        // Otherwise, fill the unused tail with holes.
        int old_length = FastD2IChecked(array->length()->Number());
        for (int i = length; i < old_length; i++) {
          Handle<BackingStore>::cast(backing_store)->set_the_hole(i);
        }
      }
      return length_object;
    }

    // Check whether the backing store should be expanded.
    uint32_t min = JSObject::NewElementsCapacity(old_capacity);
    uint32_t new_capacity = length > min ? length : min;
    if (!array->ShouldConvertToSlowElements(new_capacity)) {
      FastElementsAccessorSubclass::
          SetFastElementsCapacityAndLength(array, new_capacity, length);
      array->ValidateElements();
      return length_object;
    }

    // Request conversion to slow elements.
    return isolate->factory()->undefined_value();
  }

  static Handle<Object> DeleteCommon(Handle<JSObject> obj,
                                     uint32_t key,
                                     JSReceiver::DeleteMode mode) {
    ASSERT(obj->HasFastSmiOrObjectElements() ||
           obj->HasFastDoubleElements() ||
           obj->HasFastArgumentsElements());
    Isolate* isolate = obj->GetIsolate();
    Heap* heap = obj->GetHeap();
    Handle<FixedArrayBase> elements(obj->elements());
    if (*elements == heap->empty_fixed_array()) {
      return isolate->factory()->true_value();
    }
    Handle<BackingStore> backing_store = Handle<BackingStore>::cast(elements);
    bool is_sloppy_arguments_elements_map =
        backing_store->map() == heap->sloppy_arguments_elements_map();
    if (is_sloppy_arguments_elements_map) {
      backing_store = handle(
          BackingStore::cast(Handle<FixedArray>::cast(backing_store)->get(1)));
    }
    uint32_t length = static_cast<uint32_t>(
        obj->IsJSArray()
        ? Smi::cast(Handle<JSArray>::cast(obj)->length())->value()
        : backing_store->length());
    if (key < length) {
      if (!is_sloppy_arguments_elements_map) {
        ElementsKind kind = KindTraits::Kind;
        if (IsFastPackedElementsKind(kind)) {
          JSObject::TransitionElementsKind(obj, GetHoleyElementsKind(kind));
        }
        if (IsFastSmiOrObjectElementsKind(KindTraits::Kind)) {
          Handle<Object> writable = JSObject::EnsureWritableFastElements(obj);
          backing_store = Handle<BackingStore>::cast(writable);
        }
      }
      backing_store->set_the_hole(key);
      // If an old space backing store is larger than a certain size and
      // has too few used values, normalize it.
      // To avoid doing the check on every delete we require at least
      // one adjacent hole to the value being deleted.
      const int kMinLengthForSparsenessCheck = 64;
      if (backing_store->length() >= kMinLengthForSparsenessCheck &&
          !heap->InNewSpace(*backing_store) &&
          ((key > 0 && backing_store->is_the_hole(key - 1)) ||
           (key + 1 < length && backing_store->is_the_hole(key + 1)))) {
        int num_used = 0;
        for (int i = 0; i < backing_store->length(); ++i) {
          if (!backing_store->is_the_hole(i)) ++num_used;
          // Bail out early if more than 1/4 is used.
          if (4 * num_used > backing_store->length()) break;
        }
        if (4 * num_used <= backing_store->length()) {
          JSObject::NormalizeElements(obj);
        }
      }
    }
    return isolate->factory()->true_value();
  }

  virtual Handle<Object> Delete(
      Handle<JSObject> obj,
      uint32_t key,
      JSReceiver::DeleteMode mode) V8_FINAL V8_OVERRIDE {
    return DeleteCommon(obj, key, mode);
  }

  static bool HasElementImpl(
      Object* receiver,
      JSObject* holder,
      uint32_t key,
      FixedArrayBase* backing_store) {
    if (key >= static_cast<uint32_t>(backing_store->length())) {
      return false;
    }
    return !BackingStore::cast(backing_store)->is_the_hole(key);
  }

  static void ValidateContents(JSObject* holder, int length) {
#if DEBUG
    FixedArrayBase* elements = holder->elements();
    Heap* heap = elements->GetHeap();
    Map* map = elements->map();
    ASSERT((IsFastSmiOrObjectElementsKind(KindTraits::Kind) &&
            (map == heap->fixed_array_map() ||
             map == heap->fixed_cow_array_map())) ||
           (IsFastDoubleElementsKind(KindTraits::Kind) ==
            ((map == heap->fixed_array_map() && length == 0) ||
             map == heap->fixed_double_array_map())));
    for (int i = 0; i < length; i++) {
      BackingStore* backing_store = BackingStore::cast(elements);
      ASSERT((!IsFastSmiElementsKind(KindTraits::Kind) ||
              static_cast<Object*>(backing_store->get(i))->IsSmi()) ||
             (IsFastHoleyElementsKind(KindTraits::Kind) ==
              backing_store->is_the_hole(i)));
    }
#endif
  }
};


static inline ElementsKind ElementsKindForArray(FixedArrayBase* array) {
  switch (array->map()->instance_type()) {
    case FIXED_ARRAY_TYPE:
      if (array->IsDictionary()) {
        return DICTIONARY_ELEMENTS;
      } else {
        return FAST_HOLEY_ELEMENTS;
      }
    case FIXED_DOUBLE_ARRAY_TYPE:
      return FAST_HOLEY_DOUBLE_ELEMENTS;

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                       \
    case EXTERNAL_##TYPE##_ARRAY_TYPE:                                        \
      return EXTERNAL_##TYPE##_ELEMENTS;                                      \
    case FIXED_##TYPE##_ARRAY_TYPE:                                           \
      return TYPE##_ELEMENTS;

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    default:
      UNREACHABLE();
  }
  return FAST_HOLEY_ELEMENTS;
}


template<typename FastElementsAccessorSubclass,
         typename KindTraits>
class FastSmiOrObjectElementsAccessor
    : public FastElementsAccessor<FastElementsAccessorSubclass,
                                  KindTraits,
                                  kPointerSize> {
 public:
  explicit FastSmiOrObjectElementsAccessor(const char* name)
      : FastElementsAccessor<FastElementsAccessorSubclass,
                             KindTraits,
                             kPointerSize>(name) {}

  static void CopyElementsImpl(Handle<FixedArrayBase> from,
                               uint32_t from_start,
                               Handle<FixedArrayBase> to,
                               ElementsKind from_kind,
                               uint32_t to_start,
                               int packed_size,
                               int copy_size) {
    ElementsKind to_kind = KindTraits::Kind;
    switch (from_kind) {
      case FAST_SMI_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
        CopyObjectToObjectElements(
            from, from_kind, from_start, to, to_kind, to_start, copy_size);
        break;
      case FAST_DOUBLE_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
        CopyDoubleToObjectElements(
            from, from_start, to, to_kind, to_start, copy_size);
        break;
      case DICTIONARY_ELEMENTS:
        CopyDictionaryToObjectElements(
            from, from_start, to, to_kind, to_start, copy_size);
        break;
      case SLOPPY_ARGUMENTS_ELEMENTS: {
        // TODO(verwaest): This is a temporary hack to support extending
        // SLOPPY_ARGUMENTS_ELEMENTS in SetFastElementsCapacityAndLength.
        // This case should be UNREACHABLE().
        Handle<FixedArray> parameter_map = Handle<FixedArray>::cast(from);
        Handle<FixedArrayBase> arguments(
            FixedArrayBase::cast(parameter_map->get(1)));
        ElementsKind from_kind = ElementsKindForArray(*arguments);
        CopyElementsImpl(arguments, from_start, to, from_kind,
                         to_start, packed_size, copy_size);
        break;
      }
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                       \
      case EXTERNAL_##TYPE##_ELEMENTS:                                        \
      case TYPE##_ELEMENTS:                                                   \
        UNREACHABLE();
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    }
  }


  static void SetFastElementsCapacityAndLength(
      Handle<JSObject> obj,
      uint32_t capacity,
      uint32_t length) {
    JSObject::SetFastElementsCapacitySmiMode set_capacity_mode =
        obj->HasFastSmiElements()
            ? JSObject::kAllowSmiElements
            : JSObject::kDontAllowSmiElements;
    JSObject::SetFastElementsCapacityAndLength(
        obj, capacity, length, set_capacity_mode);
  }
};


class FastPackedSmiElementsAccessor
    : public FastSmiOrObjectElementsAccessor<
        FastPackedSmiElementsAccessor,
        ElementsKindTraits<FAST_SMI_ELEMENTS> > {
 public:
  explicit FastPackedSmiElementsAccessor(const char* name)
      : FastSmiOrObjectElementsAccessor<
          FastPackedSmiElementsAccessor,
          ElementsKindTraits<FAST_SMI_ELEMENTS> >(name) {}
};


class FastHoleySmiElementsAccessor
    : public FastSmiOrObjectElementsAccessor<
        FastHoleySmiElementsAccessor,
        ElementsKindTraits<FAST_HOLEY_SMI_ELEMENTS> > {
 public:
  explicit FastHoleySmiElementsAccessor(const char* name)
      : FastSmiOrObjectElementsAccessor<
          FastHoleySmiElementsAccessor,
          ElementsKindTraits<FAST_HOLEY_SMI_ELEMENTS> >(name) {}
};


class FastPackedObjectElementsAccessor
    : public FastSmiOrObjectElementsAccessor<
        FastPackedObjectElementsAccessor,
        ElementsKindTraits<FAST_ELEMENTS> > {
 public:
  explicit FastPackedObjectElementsAccessor(const char* name)
      : FastSmiOrObjectElementsAccessor<
          FastPackedObjectElementsAccessor,
          ElementsKindTraits<FAST_ELEMENTS> >(name) {}
};


class FastHoleyObjectElementsAccessor
    : public FastSmiOrObjectElementsAccessor<
        FastHoleyObjectElementsAccessor,
        ElementsKindTraits<FAST_HOLEY_ELEMENTS> > {
 public:
  explicit FastHoleyObjectElementsAccessor(const char* name)
      : FastSmiOrObjectElementsAccessor<
          FastHoleyObjectElementsAccessor,
          ElementsKindTraits<FAST_HOLEY_ELEMENTS> >(name) {}
};


template<typename FastElementsAccessorSubclass,
         typename KindTraits>
class FastDoubleElementsAccessor
    : public FastElementsAccessor<FastElementsAccessorSubclass,
                                  KindTraits,
                                  kDoubleSize> {
 public:
  explicit FastDoubleElementsAccessor(const char* name)
      : FastElementsAccessor<FastElementsAccessorSubclass,
                             KindTraits,
                             kDoubleSize>(name) {}

  static void SetFastElementsCapacityAndLength(Handle<JSObject> obj,
                                               uint32_t capacity,
                                               uint32_t length) {
    JSObject::SetFastDoubleElementsCapacityAndLength(obj, capacity, length);
  }

 protected:
  static void CopyElementsImpl(Handle<FixedArrayBase> from,
                               uint32_t from_start,
                               Handle<FixedArrayBase> to,
                               ElementsKind from_kind,
                               uint32_t to_start,
                               int packed_size,
                               int copy_size) {
    switch (from_kind) {
      case FAST_SMI_ELEMENTS:
        CopyPackedSmiToDoubleElements(
            from, from_start, to, to_start, packed_size, copy_size);
        break;
      case FAST_HOLEY_SMI_ELEMENTS:
        CopySmiToDoubleElements(from, from_start, to, to_start, copy_size);
        break;
      case FAST_DOUBLE_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
        CopyDoubleToDoubleElements(from, from_start, to, to_start, copy_size);
        break;
      case FAST_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
        CopyObjectToDoubleElements(from, from_start, to, to_start, copy_size);
        break;
      case DICTIONARY_ELEMENTS:
        CopyDictionaryToDoubleElements(
            from, from_start, to, to_start, copy_size);
        break;
      case SLOPPY_ARGUMENTS_ELEMENTS:
        UNREACHABLE();

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                       \
      case EXTERNAL_##TYPE##_ELEMENTS:                                        \
      case TYPE##_ELEMENTS:                                                   \
        UNREACHABLE();
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    }
  }
};


class FastPackedDoubleElementsAccessor
    : public FastDoubleElementsAccessor<
        FastPackedDoubleElementsAccessor,
        ElementsKindTraits<FAST_DOUBLE_ELEMENTS> > {
 public:
  friend class ElementsAccessorBase<FastPackedDoubleElementsAccessor,
                                    ElementsKindTraits<FAST_DOUBLE_ELEMENTS> >;
  explicit FastPackedDoubleElementsAccessor(const char* name)
      : FastDoubleElementsAccessor<
          FastPackedDoubleElementsAccessor,
          ElementsKindTraits<FAST_DOUBLE_ELEMENTS> >(name) {}
};


class FastHoleyDoubleElementsAccessor
    : public FastDoubleElementsAccessor<
        FastHoleyDoubleElementsAccessor,
        ElementsKindTraits<FAST_HOLEY_DOUBLE_ELEMENTS> > {
 public:
  friend class ElementsAccessorBase<
    FastHoleyDoubleElementsAccessor,
    ElementsKindTraits<FAST_HOLEY_DOUBLE_ELEMENTS> >;
  explicit FastHoleyDoubleElementsAccessor(const char* name)
      : FastDoubleElementsAccessor<
          FastHoleyDoubleElementsAccessor,
          ElementsKindTraits<FAST_HOLEY_DOUBLE_ELEMENTS> >(name) {}
};


// Super class for all external element arrays.
template<ElementsKind Kind>
class TypedElementsAccessor
    : public ElementsAccessorBase<TypedElementsAccessor<Kind>,
                                  ElementsKindTraits<Kind> > {
 public:
  explicit TypedElementsAccessor(const char* name)
      : ElementsAccessorBase<AccessorClass,
                             ElementsKindTraits<Kind> >(name) {}

 protected:
  typedef typename ElementsKindTraits<Kind>::BackingStore BackingStore;
  typedef TypedElementsAccessor<Kind> AccessorClass;

  friend class ElementsAccessorBase<AccessorClass,
                                    ElementsKindTraits<Kind> >;

  MUST_USE_RESULT static MaybeObject* GetImpl(Object* receiver,
                                              JSObject* obj,
                                              uint32_t key,
                                              FixedArrayBase* backing_store) {
    return
        key < AccessorClass::GetCapacityImpl(backing_store)
        ? BackingStore::cast(backing_store)->get(key)
        : backing_store->GetHeap()->undefined_value();
  }

  MUST_USE_RESULT static PropertyAttributes GetAttributesImpl(
      Object* receiver,
      JSObject* obj,
      uint32_t key,
      FixedArrayBase* backing_store) {
    return
        key < AccessorClass::GetCapacityImpl(backing_store)
          ? NONE : ABSENT;
  }

  MUST_USE_RESULT static PropertyType GetTypeImpl(
      Object* receiver,
      JSObject* obj,
      uint32_t key,
      FixedArrayBase* backing_store) {
    return
        key < AccessorClass::GetCapacityImpl(backing_store)
          ? FIELD : NONEXISTENT;
  }

  MUST_USE_RESULT static Handle<Object> SetLengthImpl(
      Handle<JSObject> obj,
      Handle<Object> length,
      Handle<FixedArrayBase> backing_store) {
    // External arrays do not support changing their length.
    UNREACHABLE();
    return obj;
  }

  MUST_USE_RESULT virtual Handle<Object> Delete(
      Handle<JSObject> obj,
      uint32_t key,
      JSReceiver::DeleteMode mode) V8_FINAL V8_OVERRIDE {
    // External arrays always ignore deletes.
    return obj->GetIsolate()->factory()->true_value();
  }

  static bool HasElementImpl(Object* receiver,
                             JSObject* holder,
                             uint32_t key,
                             FixedArrayBase* backing_store) {
    uint32_t capacity =
        AccessorClass::GetCapacityImpl(backing_store);
    return key < capacity;
  }
};



#define EXTERNAL_ELEMENTS_ACCESSOR(Type, type, TYPE, ctype, size)    \
  typedef TypedElementsAccessor<EXTERNAL_##TYPE##_ELEMENTS>          \
      External##Type##ElementsAccessor;

TYPED_ARRAYS(EXTERNAL_ELEMENTS_ACCESSOR)
#undef EXTERNAL_ELEMENTS_ACCESSOR

#define FIXED_ELEMENTS_ACCESSOR(Type, type, TYPE, ctype, size)       \
  typedef TypedElementsAccessor<TYPE##_ELEMENTS >                    \
      Fixed##Type##ElementsAccessor;

TYPED_ARRAYS(FIXED_ELEMENTS_ACCESSOR)
#undef FIXED_ELEMENTS_ACCESSOR



class DictionaryElementsAccessor
    : public ElementsAccessorBase<DictionaryElementsAccessor,
                                  ElementsKindTraits<DICTIONARY_ELEMENTS> > {
 public:
  explicit DictionaryElementsAccessor(const char* name)
      : ElementsAccessorBase<DictionaryElementsAccessor,
                             ElementsKindTraits<DICTIONARY_ELEMENTS> >(name) {}

  // Adjusts the length of the dictionary backing store and returns the new
  // length according to ES5 section 15.4.5.2 behavior.
  MUST_USE_RESULT static MaybeObject* SetLengthWithoutNormalize(
      FixedArrayBase* store,
      JSArray* array,
      Object* length_object,
      uint32_t length) {
    SeededNumberDictionary* dict = SeededNumberDictionary::cast(store);
    Heap* heap = array->GetHeap();
    int capacity = dict->Capacity();
    uint32_t new_length = length;
    uint32_t old_length = static_cast<uint32_t>(array->length()->Number());
    if (new_length < old_length) {
      // Find last non-deletable element in range of elements to be
      // deleted and adjust range accordingly.
      for (int i = 0; i < capacity; i++) {
        Object* key = dict->KeyAt(i);
        if (key->IsNumber()) {
          uint32_t number = static_cast<uint32_t>(key->Number());
          if (new_length <= number && number < old_length) {
            PropertyDetails details = dict->DetailsAt(i);
            if (details.IsDontDelete()) new_length = number + 1;
          }
        }
      }
      if (new_length != length) {
        MaybeObject* maybe_object = heap->NumberFromUint32(new_length);
        if (!maybe_object->To(&length_object)) return maybe_object;
      }
    }

    if (new_length == 0) {
      // If the length of a slow array is reset to zero, we clear
      // the array and flush backing storage. This has the added
      // benefit that the array returns to fast mode.
      Object* obj;
      MaybeObject* maybe_obj = array->ResetElements();
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    } else {
      // Remove elements that should be deleted.
      int removed_entries = 0;
      Object* the_hole_value = heap->the_hole_value();
      for (int i = 0; i < capacity; i++) {
        Object* key = dict->KeyAt(i);
        if (key->IsNumber()) {
          uint32_t number = static_cast<uint32_t>(key->Number());
          if (new_length <= number && number < old_length) {
            dict->SetEntry(i, the_hole_value, the_hole_value);
            removed_entries++;
          }
        }
      }

      // Update the number of elements.
      dict->ElementsRemoved(removed_entries);
    }
    return length_object;
  }

  // TODO(ishell): Temporary wrapper until handlified.
  MUST_USE_RESULT static Handle<Object> SetLengthWithoutNormalize(
      Handle<FixedArrayBase> store,
      Handle<JSArray> array,
      Handle<Object> length_object,
      uint32_t length) {
    CALL_HEAP_FUNCTION(array->GetIsolate(),
                       SetLengthWithoutNormalize(
                             *store, *array, *length_object, length),
                       Object);
  }

  MUST_USE_RESULT static MaybeObject* DeleteCommon(
      JSObject* obj,
      uint32_t key,
      JSReceiver::DeleteMode mode) {
    Isolate* isolate = obj->GetIsolate();
    Heap* heap = isolate->heap();
    FixedArray* backing_store = FixedArray::cast(obj->elements());
    bool is_arguments =
        (obj->GetElementsKind() == SLOPPY_ARGUMENTS_ELEMENTS);
    if (is_arguments) {
      backing_store = FixedArray::cast(backing_store->get(1));
    }
    SeededNumberDictionary* dictionary =
        SeededNumberDictionary::cast(backing_store);
    int entry = dictionary->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound) {
      Object* result = dictionary->DeleteProperty(entry, mode);
      if (result == heap->false_value()) {
        if (mode == JSObject::STRICT_DELETION) {
          // Deleting a non-configurable property in strict mode.
          HandleScope scope(isolate);
          Handle<Object> holder(obj, isolate);
          Handle<Object> name = isolate->factory()->NewNumberFromUint(key);
          Handle<Object> args[2] = { name, holder };
          Handle<Object> error =
              isolate->factory()->NewTypeError("strict_delete_property",
                                               HandleVector(args, 2));
          return isolate->Throw(*error);
        }
        return heap->false_value();
      }
      MaybeObject* maybe_elements = dictionary->Shrink(key);
      FixedArray* new_elements = NULL;
      if (!maybe_elements->To(&new_elements)) {
        return maybe_elements;
      }
      if (is_arguments) {
        FixedArray::cast(obj->elements())->set(1, new_elements);
      } else {
        obj->set_elements(new_elements);
      }
    }
    return heap->true_value();
  }

  // TODO(ishell): Temporary wrapper until handlified.
  MUST_USE_RESULT static Handle<Object> DeleteCommon(
      Handle<JSObject> obj,
      uint32_t key,
      JSReceiver::DeleteMode mode) {
    CALL_HEAP_FUNCTION(obj->GetIsolate(),
                       DeleteCommon(*obj, key, mode),
                       Object);
  }

  static void CopyElementsImpl(Handle<FixedArrayBase> from,
                               uint32_t from_start,
                               Handle<FixedArrayBase> to,
                               ElementsKind from_kind,
                               uint32_t to_start,
                               int packed_size,
                               int copy_size) {
    UNREACHABLE();
  }


 protected:
  friend class ElementsAccessorBase<DictionaryElementsAccessor,
                                    ElementsKindTraits<DICTIONARY_ELEMENTS> >;

  MUST_USE_RESULT virtual Handle<Object> Delete(
      Handle<JSObject> obj,
      uint32_t key,
      JSReceiver::DeleteMode mode) V8_FINAL V8_OVERRIDE {
    return DeleteCommon(obj, key, mode);
  }

  MUST_USE_RESULT static MaybeObject* GetImpl(
      Object* receiver,
      JSObject* obj,
      uint32_t key,
      FixedArrayBase* store) {
    SeededNumberDictionary* backing_store = SeededNumberDictionary::cast(store);
    int entry = backing_store->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound) {
      Object* element = backing_store->ValueAt(entry);
      PropertyDetails details = backing_store->DetailsAt(entry);
      if (details.type() == CALLBACKS) {
        return obj->GetElementWithCallback(receiver,
                                           element,
                                           key,
                                           obj);
      } else {
        return element;
      }
    }
    return obj->GetHeap()->the_hole_value();
  }

  MUST_USE_RESULT static PropertyAttributes GetAttributesImpl(
      Object* receiver,
      JSObject* obj,
      uint32_t key,
      FixedArrayBase* backing_store) {
    SeededNumberDictionary* dictionary =
        SeededNumberDictionary::cast(backing_store);
    int entry = dictionary->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound) {
      return dictionary->DetailsAt(entry).attributes();
    }
    return ABSENT;
  }

  MUST_USE_RESULT static PropertyType GetTypeImpl(
      Object* receiver,
      JSObject* obj,
      uint32_t key,
      FixedArrayBase* store) {
    SeededNumberDictionary* backing_store = SeededNumberDictionary::cast(store);
    int entry = backing_store->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound) {
      return backing_store->DetailsAt(entry).type();
    }
    return NONEXISTENT;
  }

  MUST_USE_RESULT static AccessorPair* GetAccessorPairImpl(
      Object* receiver,
      JSObject* obj,
      uint32_t key,
      FixedArrayBase* store) {
    SeededNumberDictionary* backing_store = SeededNumberDictionary::cast(store);
    int entry = backing_store->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound &&
        backing_store->DetailsAt(entry).type() == CALLBACKS &&
        backing_store->ValueAt(entry)->IsAccessorPair()) {
      return AccessorPair::cast(backing_store->ValueAt(entry));
    }
    return NULL;
  }

  static bool HasElementImpl(Object* receiver,
                             JSObject* holder,
                             uint32_t key,
                             FixedArrayBase* backing_store) {
    return SeededNumberDictionary::cast(backing_store)->FindEntry(key) !=
        SeededNumberDictionary::kNotFound;
  }

  static uint32_t GetKeyForIndexImpl(FixedArrayBase* store,
                                     uint32_t index) {
    SeededNumberDictionary* dict = SeededNumberDictionary::cast(store);
    Object* key = dict->KeyAt(index);
    return Smi::cast(key)->value();
  }
};


class SloppyArgumentsElementsAccessor : public ElementsAccessorBase<
    SloppyArgumentsElementsAccessor,
    ElementsKindTraits<SLOPPY_ARGUMENTS_ELEMENTS> > {
 public:
  explicit SloppyArgumentsElementsAccessor(const char* name)
      : ElementsAccessorBase<
          SloppyArgumentsElementsAccessor,
          ElementsKindTraits<SLOPPY_ARGUMENTS_ELEMENTS> >(name) {}
 protected:
  friend class ElementsAccessorBase<
      SloppyArgumentsElementsAccessor,
      ElementsKindTraits<SLOPPY_ARGUMENTS_ELEMENTS> >;

  MUST_USE_RESULT static MaybeObject* GetImpl(Object* receiver,
                                              JSObject* obj,
                                              uint32_t key,
                                              FixedArrayBase* parameters) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    Object* probe = GetParameterMapArg(obj, parameter_map, key);
    if (!probe->IsTheHole()) {
      Context* context = Context::cast(parameter_map->get(0));
      int context_index = Smi::cast(probe)->value();
      ASSERT(!context->get(context_index)->IsTheHole());
      return context->get(context_index);
    } else {
      // Object is not mapped, defer to the arguments.
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      MaybeObject* maybe_result = ElementsAccessor::ForArray(arguments)->Get(
          receiver, obj, key, arguments);
      Object* result;
      if (!maybe_result->ToObject(&result)) return maybe_result;
      // Elements of the arguments object in slow mode might be slow aliases.
      if (result->IsAliasedArgumentsEntry()) {
        AliasedArgumentsEntry* entry = AliasedArgumentsEntry::cast(result);
        Context* context = Context::cast(parameter_map->get(0));
        int context_index = entry->aliased_context_slot();
        ASSERT(!context->get(context_index)->IsTheHole());
        return context->get(context_index);
      } else {
        return result;
      }
    }
  }

  MUST_USE_RESULT static PropertyAttributes GetAttributesImpl(
      Object* receiver,
      JSObject* obj,
      uint32_t key,
      FixedArrayBase* backing_store) {
    FixedArray* parameter_map = FixedArray::cast(backing_store);
    Object* probe = GetParameterMapArg(obj, parameter_map, key);
    if (!probe->IsTheHole()) {
      return NONE;
    } else {
      // If not aliased, check the arguments.
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      return ElementsAccessor::ForArray(arguments)->GetAttributes(
          receiver, obj, key, arguments);
    }
  }

  MUST_USE_RESULT static PropertyType GetTypeImpl(
      Object* receiver,
      JSObject* obj,
      uint32_t key,
      FixedArrayBase* parameters) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    Object* probe = GetParameterMapArg(obj, parameter_map, key);
    if (!probe->IsTheHole()) {
      return FIELD;
    } else {
      // If not aliased, check the arguments.
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      return ElementsAccessor::ForArray(arguments)->GetType(
          receiver, obj, key, arguments);
    }
  }

  MUST_USE_RESULT static AccessorPair* GetAccessorPairImpl(
      Object* receiver,
      JSObject* obj,
      uint32_t key,
      FixedArrayBase* parameters) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    Object* probe = GetParameterMapArg(obj, parameter_map, key);
    if (!probe->IsTheHole()) {
      return NULL;
    } else {
      // If not aliased, check the arguments.
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      return ElementsAccessor::ForArray(arguments)->GetAccessorPair(
          receiver, obj, key, arguments);
    }
  }

  MUST_USE_RESULT static Handle<Object> SetLengthImpl(
      Handle<JSObject> obj,
      Handle<Object> length,
      Handle<FixedArrayBase> parameter_map) {
    // TODO(mstarzinger): This was never implemented but will be used once we
    // correctly implement [[DefineOwnProperty]] on arrays.
    UNIMPLEMENTED();
    return obj;
  }

  MUST_USE_RESULT virtual Handle<Object> Delete(
      Handle<JSObject> obj,
      uint32_t key,
      JSReceiver::DeleteMode mode) V8_FINAL V8_OVERRIDE {
    Isolate* isolate = obj->GetIsolate();
    Handle<FixedArray> parameter_map(FixedArray::cast(obj->elements()));
    Handle<Object> probe = GetParameterMapArg(obj, parameter_map, key);
    if (!probe->IsTheHole()) {
      // TODO(kmillikin): We could check if this was the last aliased
      // parameter, and revert to normal elements in that case.  That
      // would enable GC of the context.
      parameter_map->set_the_hole(key + 2);
    } else {
      Handle<FixedArray> arguments(FixedArray::cast(parameter_map->get(1)));
      if (arguments->IsDictionary()) {
        return DictionaryElementsAccessor::DeleteCommon(obj, key, mode);
      } else {
        // It's difficult to access the version of DeleteCommon that is declared
        // in the templatized super class, call the concrete implementation in
        // the class for the most generalized ElementsKind subclass.
        return FastHoleyObjectElementsAccessor::DeleteCommon(obj, key, mode);
      }
    }
    return isolate->factory()->true_value();
  }

  static void CopyElementsImpl(Handle<FixedArrayBase> from,
                               uint32_t from_start,
                               Handle<FixedArrayBase> to,
                               ElementsKind from_kind,
                               uint32_t to_start,
                               int packed_size,
                               int copy_size) {
    UNREACHABLE();
  }

  static uint32_t GetCapacityImpl(FixedArrayBase* backing_store) {
    FixedArray* parameter_map = FixedArray::cast(backing_store);
    FixedArrayBase* arguments = FixedArrayBase::cast(parameter_map->get(1));
    return Max(static_cast<uint32_t>(parameter_map->length() - 2),
               ForArray(arguments)->GetCapacity(arguments));
  }

  static uint32_t GetKeyForIndexImpl(FixedArrayBase* dict,
                                     uint32_t index) {
    return index;
  }

  static bool HasElementImpl(Object* receiver,
                             JSObject* holder,
                             uint32_t key,
                             FixedArrayBase* parameters) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    Object* probe = GetParameterMapArg(holder, parameter_map, key);
    if (!probe->IsTheHole()) {
      return true;
    } else {
      FixedArrayBase* arguments =
          FixedArrayBase::cast(FixedArray::cast(parameter_map)->get(1));
      ElementsAccessor* accessor = ElementsAccessor::ForArray(arguments);
      return !accessor->Get(receiver, holder, key, arguments)->IsTheHole();
    }
  }

 private:
  // TODO(ishell): remove when all usages are handlified.
  static Object* GetParameterMapArg(JSObject* holder,
                                    FixedArray* parameter_map,
                                    uint32_t key) {
    uint32_t length = holder->IsJSArray()
        ? Smi::cast(JSArray::cast(holder)->length())->value()
        : parameter_map->length();
    return key < (length - 2)
        ? parameter_map->get(key + 2)
        : parameter_map->GetHeap()->the_hole_value();
  }

  static Handle<Object> GetParameterMapArg(Handle<JSObject> holder,
                                           Handle<FixedArray> parameter_map,
                                           uint32_t key) {
    Isolate* isolate = holder->GetIsolate();
    uint32_t length = holder->IsJSArray()
        ? Smi::cast(Handle<JSArray>::cast(holder)->length())->value()
        : parameter_map->length();
    return key < (length - 2)
        ? handle(parameter_map->get(key + 2), isolate)
        : Handle<Object>::cast(isolate->factory()->the_hole_value());
  }
};


ElementsAccessor* ElementsAccessor::ForArray(FixedArrayBase* array) {
  return elements_accessors_[ElementsKindForArray(array)];
}


void ElementsAccessor::InitializeOncePerProcess() {
  static ElementsAccessor* accessor_array[] = {
#define ACCESSOR_ARRAY(Class, Kind, Store) new Class(#Kind),
    ELEMENTS_LIST(ACCESSOR_ARRAY)
#undef ACCESSOR_ARRAY
  };

  STATIC_ASSERT((sizeof(accessor_array) / sizeof(*accessor_array)) ==
                kElementsKindCount);

  elements_accessors_ = accessor_array;
}


void ElementsAccessor::TearDown() {
#define ACCESSOR_DELETE(Class, Kind, Store) delete elements_accessors_[Kind];
  ELEMENTS_LIST(ACCESSOR_DELETE)
#undef ACCESSOR_DELETE
  elements_accessors_ = NULL;
}


template <typename ElementsAccessorSubclass, typename ElementsKindTraits>
MUST_USE_RESULT Handle<Object> ElementsAccessorBase<ElementsAccessorSubclass,
                                                    ElementsKindTraits>::
    SetLengthImpl(Handle<JSObject> obj,
                  Handle<Object> length,
                  Handle<FixedArrayBase> backing_store) {
  Isolate* isolate = obj->GetIsolate();
  Handle<JSArray> array = Handle<JSArray>::cast(obj);

  // Fast case: The new length fits into a Smi.
  Handle<Object> smi_length = Object::ToSmi(isolate, length);

  if (!smi_length.is_null() && smi_length->IsSmi()) {
    const int value = Handle<Smi>::cast(smi_length)->value();
    if (value >= 0) {
      Handle<Object> new_length = ElementsAccessorSubclass::
          SetLengthWithoutNormalize(backing_store, array, smi_length, value);
      RETURN_IF_EMPTY_HANDLE_VALUE(isolate, new_length, new_length);

      // even though the proposed length was a smi, new_length could
      // still be a heap number because SetLengthWithoutNormalize doesn't
      // allow the array length property to drop below the index of
      // non-deletable elements.
      ASSERT(new_length->IsSmi() || new_length->IsHeapNumber() ||
             new_length->IsUndefined());
      if (new_length->IsSmi()) {
        array->set_length(*Handle<Smi>::cast(new_length));
        return array;
      } else if (new_length->IsHeapNumber()) {
        array->set_length(*new_length);
        return array;
      }
    } else {
      return ThrowArrayLengthRangeError(isolate);
    }
  }

  // Slow case: The new length does not fit into a Smi or conversion
  // to slow elements is needed for other reasons.
  if (length->IsNumber()) {
    uint32_t value;
    if (length->ToArrayIndex(&value)) {
      Handle<SeededNumberDictionary> dictionary =
          JSObject::NormalizeElements(array);
      RETURN_IF_EMPTY_HANDLE_VALUE(isolate, dictionary, dictionary);

      Handle<Object> new_length = DictionaryElementsAccessor::
          SetLengthWithoutNormalize(dictionary, array, length, value);
      RETURN_IF_EMPTY_HANDLE_VALUE(isolate, new_length, new_length);

      ASSERT(new_length->IsNumber());
      array->set_length(*new_length);
      return array;
    } else {
      return ThrowArrayLengthRangeError(isolate);
    }
  }

  // Fall-back case: The new length is not a number so make the array
  // size one and set only element to length.
  Handle<FixedArray> new_backing_store = isolate->factory()->NewFixedArray(1);
  new_backing_store->set(0, *length);
  JSArray::SetContent(array, new_backing_store);
  return array;
}


Handle<Object> ArrayConstructInitializeElements(Handle<JSArray> array,
                                                Arguments* args) {
  // Optimize the case where there is one argument and the argument is a
  // small smi.
  if (args->length() == 1) {
    Handle<Object> obj = args->at<Object>(0);
    if (obj->IsSmi()) {
      int len = Handle<Smi>::cast(obj)->value();
      if (len > 0 && len < JSObject::kInitialMaxFastElementArray) {
        ElementsKind elements_kind = array->GetElementsKind();
        JSArray::Initialize(array, len, len);

        if (!IsFastHoleyElementsKind(elements_kind)) {
          elements_kind = GetHoleyElementsKind(elements_kind);
          JSObject::TransitionElementsKind(array, elements_kind);
        }
        return array;
      } else if (len == 0) {
        JSArray::Initialize(array, JSArray::kPreallocatedArrayElements);
        return array;
      }
    }

    // Take the argument as the length.
    JSArray::Initialize(array, 0);

    return JSArray::SetElementsLength(array, obj);
  }

  // Optimize the case where there are no parameters passed.
  if (args->length() == 0) {
    JSArray::Initialize(array, JSArray::kPreallocatedArrayElements);
    return array;
  }

  Factory* factory = array->GetIsolate()->factory();

  // Set length and elements on the array.
  int number_of_elements = args->length();
  JSObject::EnsureCanContainElements(
      array, args, 0, number_of_elements, ALLOW_CONVERTED_DOUBLE_ELEMENTS);

  // Allocate an appropriately typed elements array.
  ElementsKind elements_kind = array->GetElementsKind();
  Handle<FixedArrayBase> elms;
  if (IsFastDoubleElementsKind(elements_kind)) {
    elms = Handle<FixedArrayBase>::cast(
        factory->NewFixedDoubleArray(number_of_elements));
  } else {
    elms = Handle<FixedArrayBase>::cast(
        factory->NewFixedArrayWithHoles(number_of_elements));
  }

  // Fill in the content
  switch (array->GetElementsKind()) {
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_SMI_ELEMENTS: {
      Handle<FixedArray> smi_elms = Handle<FixedArray>::cast(elms);
      for (int index = 0; index < number_of_elements; index++) {
        smi_elms->set(index, (*args)[index], SKIP_WRITE_BARRIER);
      }
      break;
    }
    case FAST_HOLEY_ELEMENTS:
    case FAST_ELEMENTS: {
      DisallowHeapAllocation no_gc;
      WriteBarrierMode mode = elms->GetWriteBarrierMode(no_gc);
      Handle<FixedArray> object_elms = Handle<FixedArray>::cast(elms);
      for (int index = 0; index < number_of_elements; index++) {
        object_elms->set(index, (*args)[index], mode);
      }
      break;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      Handle<FixedDoubleArray> double_elms =
          Handle<FixedDoubleArray>::cast(elms);
      for (int index = 0; index < number_of_elements; index++) {
        double_elms->set(index, (*args)[index]->Number());
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }

  array->set_elements(*elms);
  array->set_length(Smi::FromInt(number_of_elements));
  return array;
}

} }  // namespace v8::internal

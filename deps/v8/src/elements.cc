// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/conversions.h"
#include "src/elements.h"
#include "src/objects.h"
#include "src/utils.h"

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
 public:   /* NOLINT */                                        \
  static const ElementsKind Kind = KindParam;                  \
  typedef Store BackingStore;                                  \
};
ELEMENTS_LIST(ELEMENTS_TRAITS)
#undef ELEMENTS_TRAITS


ElementsAccessor** ElementsAccessor::elements_accessors_ = NULL;


static bool HasKey(Handle<FixedArray> array, Handle<Object> key_handle) {
  DisallowHeapAllocation no_gc;
  Object* key = *key_handle;
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


MUST_USE_RESULT
static MaybeHandle<Object> ThrowArrayLengthRangeError(Isolate* isolate) {
  THROW_NEW_ERROR(isolate, NewRangeError("invalid_array_length",
                                         HandleVector<Object>(NULL, 0)),
                  Object);
}


static void CopyObjectToObjectElements(FixedArrayBase* from_base,
                                       ElementsKind from_kind,
                                       uint32_t from_start,
                                       FixedArrayBase* to_base,
                                       ElementsKind to_kind, uint32_t to_start,
                                       int raw_copy_size) {
  DCHECK(to_base->map() !=
      from_base->GetIsolate()->heap()->fixed_cow_array_map());
  DisallowHeapAllocation no_allocation;
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    DCHECK(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = Min(from_base->length() - from_start,
                    to_base->length() - to_start);
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      int start = to_start + copy_size;
      int length = to_base->length() - start;
      if (length > 0) {
        Heap* heap = from_base->GetHeap();
        MemsetPointer(FixedArray::cast(to_base)->data_start() + start,
                      heap->the_hole_value(), length);
      }
    }
  }
  DCHECK((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  FixedArray* from = FixedArray::cast(from_base);
  FixedArray* to = FixedArray::cast(to_base);
  DCHECK(IsFastSmiOrObjectElementsKind(from_kind));
  DCHECK(IsFastSmiOrObjectElementsKind(to_kind));
  Address to_address = to->address() + FixedArray::kHeaderSize;
  Address from_address = from->address() + FixedArray::kHeaderSize;
  CopyWords(reinterpret_cast<Object**>(to_address) + to_start,
            reinterpret_cast<Object**>(from_address) + from_start,
            static_cast<size_t>(copy_size));
  if (IsFastObjectElementsKind(from_kind) &&
      IsFastObjectElementsKind(to_kind)) {
    Heap* heap = from->GetHeap();
    if (!heap->InNewSpace(to)) {
      heap->RecordWrites(to->address(),
                         to->OffsetOfElementAt(to_start),
                         copy_size);
    }
    heap->incremental_marking()->RecordWrites(to);
  }
}


static void CopyDictionaryToObjectElements(
    FixedArrayBase* from_base, uint32_t from_start, FixedArrayBase* to_base,
    ElementsKind to_kind, uint32_t to_start, int raw_copy_size) {
  DisallowHeapAllocation no_allocation;
  SeededNumberDictionary* from = SeededNumberDictionary::cast(from_base);
  int copy_size = raw_copy_size;
  Heap* heap = from->GetHeap();
  if (raw_copy_size < 0) {
    DCHECK(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from->max_number_key() + 1 - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      int start = to_start + copy_size;
      int length = to_base->length() - start;
      if (length > 0) {
        Heap* heap = from->GetHeap();
        MemsetPointer(FixedArray::cast(to_base)->data_start() + start,
                      heap->the_hole_value(), length);
      }
    }
  }
  DCHECK(to_base != from_base);
  DCHECK(IsFastSmiOrObjectElementsKind(to_kind));
  if (copy_size == 0) return;
  FixedArray* to = FixedArray::cast(to_base);
  uint32_t to_length = to->length();
  if (to_start + copy_size > to_length) {
    copy_size = to_length - to_start;
  }
  for (int i = 0; i < copy_size; i++) {
    int entry = from->FindEntry(i + from_start);
    if (entry != SeededNumberDictionary::kNotFound) {
      Object* value = from->ValueAt(entry);
      DCHECK(!value->IsTheHole());
      to->set(i + to_start, value, SKIP_WRITE_BARRIER);
    } else {
      to->set_the_hole(i + to_start);
    }
  }
  if (IsFastObjectElementsKind(to_kind)) {
    if (!heap->InNewSpace(to)) {
      heap->RecordWrites(to->address(),
                         to->OffsetOfElementAt(to_start),
                         copy_size);
    }
    heap->incremental_marking()->RecordWrites(to);
  }
}


// NOTE: this method violates the handlified function signature convention:
// raw pointer parameters in the function that allocates.
// See ElementsAccessorBase::CopyElements() for details.
static void CopyDoubleToObjectElements(FixedArrayBase* from_base,
                                       uint32_t from_start,
                                       FixedArrayBase* to_base,
                                       ElementsKind to_kind, uint32_t to_start,
                                       int raw_copy_size) {
  DCHECK(IsFastSmiOrObjectElementsKind(to_kind));
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    DisallowHeapAllocation no_allocation;
    DCHECK(raw_copy_size == ElementsAccessor::kCopyToEnd ||
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
        MemsetPointer(FixedArray::cast(to_base)->data_start() + start,
                      heap->the_hole_value(), length);
      }
    }
  }
  DCHECK((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;

  // From here on, the code below could actually allocate. Therefore the raw
  // values are wrapped into handles.
  Isolate* isolate = from_base->GetIsolate();
  Handle<FixedDoubleArray> from(FixedDoubleArray::cast(from_base), isolate);
  Handle<FixedArray> to(FixedArray::cast(to_base), isolate);
  for (int i = 0; i < copy_size; ++i) {
    HandleScope scope(isolate);
    if (IsFastSmiElementsKind(to_kind)) {
      UNIMPLEMENTED();
    } else {
      DCHECK(IsFastObjectElementsKind(to_kind));
      Handle<Object> value = FixedDoubleArray::get(from, i + from_start);
      to->set(i + to_start, *value, UPDATE_WRITE_BARRIER);
    }
  }
}


static void CopyDoubleToDoubleElements(FixedArrayBase* from_base,
                                       uint32_t from_start,
                                       FixedArrayBase* to_base,
                                       uint32_t to_start, int raw_copy_size) {
  DisallowHeapAllocation no_allocation;
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    DCHECK(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = Min(from_base->length() - from_start,
                    to_base->length() - to_start);
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to_base->length(); ++i) {
        FixedDoubleArray::cast(to_base)->set_the_hole(i);
      }
    }
  }
  DCHECK((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  FixedDoubleArray* from = FixedDoubleArray::cast(from_base);
  FixedDoubleArray* to = FixedDoubleArray::cast(to_base);
  Address to_address = to->address() + FixedDoubleArray::kHeaderSize;
  Address from_address = from->address() + FixedDoubleArray::kHeaderSize;
  to_address += kDoubleSize * to_start;
  from_address += kDoubleSize * from_start;
  int words_per_double = (kDoubleSize / kPointerSize);
  CopyWords(reinterpret_cast<Object**>(to_address),
            reinterpret_cast<Object**>(from_address),
            static_cast<size_t>(words_per_double * copy_size));
}


static void CopySmiToDoubleElements(FixedArrayBase* from_base,
                                    uint32_t from_start,
                                    FixedArrayBase* to_base, uint32_t to_start,
                                    int raw_copy_size) {
  DisallowHeapAllocation no_allocation;
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    DCHECK(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from_base->length() - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to_base->length(); ++i) {
        FixedDoubleArray::cast(to_base)->set_the_hole(i);
      }
    }
  }
  DCHECK((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  FixedArray* from = FixedArray::cast(from_base);
  FixedDoubleArray* to = FixedDoubleArray::cast(to_base);
  Object* the_hole = from->GetHeap()->the_hole_value();
  for (uint32_t from_end = from_start + static_cast<uint32_t>(copy_size);
       from_start < from_end; from_start++, to_start++) {
    Object* hole_or_smi = from->get(from_start);
    if (hole_or_smi == the_hole) {
      to->set_the_hole(to_start);
    } else {
      to->set(to_start, Smi::cast(hole_or_smi)->value());
    }
  }
}


static void CopyPackedSmiToDoubleElements(FixedArrayBase* from_base,
                                          uint32_t from_start,
                                          FixedArrayBase* to_base,
                                          uint32_t to_start, int packed_size,
                                          int raw_copy_size) {
  DisallowHeapAllocation no_allocation;
  int copy_size = raw_copy_size;
  uint32_t to_end;
  if (raw_copy_size < 0) {
    DCHECK(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = packed_size - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      to_end = to_base->length();
      for (uint32_t i = to_start + copy_size; i < to_end; ++i) {
        FixedDoubleArray::cast(to_base)->set_the_hole(i);
      }
    } else {
      to_end = to_start + static_cast<uint32_t>(copy_size);
    }
  } else {
    to_end = to_start + static_cast<uint32_t>(copy_size);
  }
  DCHECK(static_cast<int>(to_end) <= to_base->length());
  DCHECK(packed_size >= 0 && packed_size <= copy_size);
  DCHECK((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  FixedArray* from = FixedArray::cast(from_base);
  FixedDoubleArray* to = FixedDoubleArray::cast(to_base);
  for (uint32_t from_end = from_start + static_cast<uint32_t>(packed_size);
       from_start < from_end; from_start++, to_start++) {
    Object* smi = from->get(from_start);
    DCHECK(!smi->IsTheHole());
    to->set(to_start, Smi::cast(smi)->value());
  }
}


static void CopyObjectToDoubleElements(FixedArrayBase* from_base,
                                       uint32_t from_start,
                                       FixedArrayBase* to_base,
                                       uint32_t to_start, int raw_copy_size) {
  DisallowHeapAllocation no_allocation;
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    DCHECK(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from_base->length() - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to_base->length(); ++i) {
        FixedDoubleArray::cast(to_base)->set_the_hole(i);
      }
    }
  }
  DCHECK((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  FixedArray* from = FixedArray::cast(from_base);
  FixedDoubleArray* to = FixedDoubleArray::cast(to_base);
  Object* the_hole = from->GetHeap()->the_hole_value();
  for (uint32_t from_end = from_start + copy_size;
       from_start < from_end; from_start++, to_start++) {
    Object* hole_or_object = from->get(from_start);
    if (hole_or_object == the_hole) {
      to->set_the_hole(to_start);
    } else {
      to->set(to_start, hole_or_object->Number());
    }
  }
}


static void CopyDictionaryToDoubleElements(FixedArrayBase* from_base,
                                           uint32_t from_start,
                                           FixedArrayBase* to_base,
                                           uint32_t to_start,
                                           int raw_copy_size) {
  DisallowHeapAllocation no_allocation;
  SeededNumberDictionary* from = SeededNumberDictionary::cast(from_base);
  int copy_size = raw_copy_size;
  if (copy_size < 0) {
    DCHECK(copy_size == ElementsAccessor::kCopyToEnd ||
           copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from->max_number_key() + 1 - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to_base->length(); ++i) {
        FixedDoubleArray::cast(to_base)->set_the_hole(i);
      }
    }
  }
  if (copy_size == 0) return;
  FixedDoubleArray* to = FixedDoubleArray::cast(to_base);
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


void CheckArrayAbuse(Handle<JSObject> obj, const char* op, uint32_t key,
                     bool allow_appending) {
  DisallowHeapAllocation no_allocation;
  Object* raw_length = NULL;
  const char* elements_type = "array";
  if (obj->IsJSArray()) {
    JSArray* array = JSArray::cast(*obj);
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

  ElementsKind kind() const FINAL { return ElementsTraits::Kind; }

  static void ValidateContents(Handle<JSObject> holder, int length) {
  }

  static void ValidateImpl(Handle<JSObject> holder) {
    Handle<FixedArrayBase> fixed_array_base(holder->elements());
    if (!fixed_array_base->IsHeapObject()) return;
    // Arrays that have been shifted in place can't be verified.
    if (fixed_array_base->IsFiller()) return;
    int length = 0;
    if (holder->IsJSArray()) {
      Object* length_obj = Handle<JSArray>::cast(holder)->length();
      if (length_obj->IsSmi()) {
        length = Smi::cast(length_obj)->value();
      }
    } else {
      length = fixed_array_base->length();
    }
    ElementsAccessorSubclass::ValidateContents(holder, length);
  }

  void Validate(Handle<JSObject> holder) FINAL {
    DisallowHeapAllocation no_gc;
    ElementsAccessorSubclass::ValidateImpl(holder);
  }

  static bool HasElementImpl(Handle<JSObject> holder, uint32_t key,
                             Handle<FixedArrayBase> backing_store) {
    return ElementsAccessorSubclass::GetAttributesImpl(holder, key,
                                                       backing_store) != ABSENT;
  }

  virtual bool HasElement(Handle<JSObject> holder, uint32_t key,
                          Handle<FixedArrayBase> backing_store) FINAL {
    return ElementsAccessorSubclass::HasElementImpl(holder, key, backing_store);
  }

  MUST_USE_RESULT virtual MaybeHandle<Object> Get(
      Handle<Object> receiver, Handle<JSObject> holder, uint32_t key,
      Handle<FixedArrayBase> backing_store) FINAL {
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

  MUST_USE_RESULT static MaybeHandle<Object> GetImpl(
      Handle<Object> receiver,
      Handle<JSObject> obj,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) {
    if (key < ElementsAccessorSubclass::GetCapacityImpl(backing_store)) {
      return BackingStore::get(Handle<BackingStore>::cast(backing_store), key);
    } else {
      return backing_store->GetIsolate()->factory()->the_hole_value();
    }
  }

  MUST_USE_RESULT virtual PropertyAttributes GetAttributes(
      Handle<JSObject> holder, uint32_t key,
      Handle<FixedArrayBase> backing_store) FINAL {
    return ElementsAccessorSubclass::GetAttributesImpl(holder, key,
                                                       backing_store);
  }

  MUST_USE_RESULT static PropertyAttributes GetAttributesImpl(
        Handle<JSObject> obj,
        uint32_t key,
        Handle<FixedArrayBase> backing_store) {
    if (key >= ElementsAccessorSubclass::GetCapacityImpl(backing_store)) {
      return ABSENT;
    }
    return
        Handle<BackingStore>::cast(backing_store)->is_the_hole(key)
          ? ABSENT : NONE;
  }

  MUST_USE_RESULT virtual MaybeHandle<AccessorPair> GetAccessorPair(
      Handle<JSObject> holder, uint32_t key,
      Handle<FixedArrayBase> backing_store) FINAL {
    return ElementsAccessorSubclass::GetAccessorPairImpl(holder, key,
                                                         backing_store);
  }

  MUST_USE_RESULT static MaybeHandle<AccessorPair> GetAccessorPairImpl(
      Handle<JSObject> obj,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) {
    return MaybeHandle<AccessorPair>();
  }

  MUST_USE_RESULT virtual MaybeHandle<Object> SetLength(
      Handle<JSArray> array, Handle<Object> length) FINAL {
    return ElementsAccessorSubclass::SetLengthImpl(
        array, length, handle(array->elements()));
  }

  MUST_USE_RESULT static MaybeHandle<Object> SetLengthImpl(
      Handle<JSObject> obj,
      Handle<Object> length,
      Handle<FixedArrayBase> backing_store);

  virtual void SetCapacityAndLength(Handle<JSArray> array, int capacity,
                                    int length) FINAL {
    ElementsAccessorSubclass::
        SetFastElementsCapacityAndLength(array, capacity, length);
  }

  static void SetFastElementsCapacityAndLength(
      Handle<JSObject> obj,
      int capacity,
      int length) {
    UNIMPLEMENTED();
  }

  MUST_USE_RESULT virtual MaybeHandle<Object> Delete(
      Handle<JSObject> obj, uint32_t key,
      LanguageMode language_mode) OVERRIDE = 0;

  static void CopyElementsImpl(FixedArrayBase* from, uint32_t from_start,
                               FixedArrayBase* to, ElementsKind from_kind,
                               uint32_t to_start, int packed_size,
                               int copy_size) {
    UNREACHABLE();
  }

  virtual void CopyElements(Handle<FixedArrayBase> from, uint32_t from_start,
                            ElementsKind from_kind, Handle<FixedArrayBase> to,
                            uint32_t to_start, int copy_size) FINAL {
    DCHECK(!from.is_null());
    // NOTE: the ElementsAccessorSubclass::CopyElementsImpl() methods
    // violate the handlified function signature convention:
    // raw pointer parameters in the function that allocates. This is done
    // intentionally to avoid ArrayConcat() builtin performance degradation.
    // See the comment in another ElementsAccessorBase::CopyElements() for
    // details.
    ElementsAccessorSubclass::CopyElementsImpl(*from, from_start, *to,
                                               from_kind, to_start,
                                               kPackedSizeNotKnown, copy_size);
  }

  virtual void CopyElements(JSObject* from_holder, uint32_t from_start,
                            ElementsKind from_kind, Handle<FixedArrayBase> to,
                            uint32_t to_start, int copy_size) FINAL {
    int packed_size = kPackedSizeNotKnown;
    bool is_packed = IsFastPackedElementsKind(from_kind) &&
        from_holder->IsJSArray();
    if (is_packed) {
      packed_size =
          Smi::cast(JSArray::cast(from_holder)->length())->value();
      if (copy_size >= 0 && packed_size > copy_size) {
        packed_size = copy_size;
      }
    }
    FixedArrayBase* from = from_holder->elements();
    // NOTE: the ElementsAccessorSubclass::CopyElementsImpl() methods
    // violate the handlified function signature convention:
    // raw pointer parameters in the function that allocates. This is done
    // intentionally to avoid ArrayConcat() builtin performance degradation.
    //
    // Details: The idea is that allocations actually happen only in case of
    // copying from object with fast double elements to object with object
    // elements. In all the other cases there are no allocations performed and
    // handle creation causes noticeable performance degradation of the builtin.
    ElementsAccessorSubclass::CopyElementsImpl(
        from, from_start, *to, from_kind, to_start, packed_size, copy_size);
  }

  virtual MaybeHandle<FixedArray> AddElementsToFixedArray(
      Handle<Object> receiver, Handle<JSObject> holder, Handle<FixedArray> to,
      Handle<FixedArrayBase> from, FixedArray::KeyFilter filter) FINAL {
    int len0 = to->length();
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      for (int i = 0; i < len0; i++) {
        DCHECK(!to->get(i)->IsTheHole());
      }
    }
#endif

    // Optimize if 'other' is empty.
    // We cannot optimize if 'this' is empty, as other may have holes.
    uint32_t len1 = ElementsAccessorSubclass::GetCapacityImpl(from);
    if (len1 == 0) return to;

    Isolate* isolate = from->GetIsolate();

    // Compute how many elements are not in other.
    uint32_t extra = 0;
    for (uint32_t y = 0; y < len1; y++) {
      uint32_t key = ElementsAccessorSubclass::GetKeyForIndexImpl(from, y);
      if (ElementsAccessorSubclass::HasElementImpl(holder, key, from)) {
        Handle<Object> value;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, value,
            ElementsAccessorSubclass::GetImpl(receiver, holder, key, from),
            FixedArray);

        DCHECK(!value->IsTheHole());
        if (filter == FixedArray::NON_SYMBOL_KEYS && value->IsSymbol()) {
          continue;
        }
        if (!HasKey(to, value)) {
          extra++;
        }
      }
    }

    if (extra == 0) return to;

    // Allocate the result
    Handle<FixedArray> result = isolate->factory()->NewFixedArray(len0 + extra);

    // Fill in the content
    {
      DisallowHeapAllocation no_gc;
      WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
      for (int i = 0; i < len0; i++) {
        Object* e = to->get(i);
        DCHECK(e->IsString() || e->IsNumber());
        result->set(i, e, mode);
      }
    }
    // Fill in the extra values.
    uint32_t index = 0;
    for (uint32_t y = 0; y < len1; y++) {
      uint32_t key =
          ElementsAccessorSubclass::GetKeyForIndexImpl(from, y);
      if (ElementsAccessorSubclass::HasElementImpl(holder, key, from)) {
        Handle<Object> value;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, value,
            ElementsAccessorSubclass::GetImpl(receiver, holder, key, from),
            FixedArray);
        if (filter == FixedArray::NON_SYMBOL_KEYS && value->IsSymbol()) {
          continue;
        }
        if (!value->IsTheHole() && !HasKey(to, value)) {
          result->set(len0 + index, *value);
          index++;
        }
      }
    }
    DCHECK(extra == index);
    return result;
  }

 protected:
  static uint32_t GetCapacityImpl(Handle<FixedArrayBase> backing_store) {
    return backing_store->length();
  }

  uint32_t GetCapacity(Handle<FixedArrayBase> backing_store) FINAL {
    return ElementsAccessorSubclass::GetCapacityImpl(backing_store);
  }

  static uint32_t GetKeyForIndexImpl(Handle<FixedArrayBase> backing_store,
                                     uint32_t index) {
    return index;
  }

  virtual uint32_t GetKeyForIndex(Handle<FixedArrayBase> backing_store,
                                  uint32_t index) FINAL {
    return ElementsAccessorSubclass::GetKeyForIndexImpl(backing_store, index);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementsAccessorBase);
};


// Super class for all fast element arrays.
template<typename FastElementsAccessorSubclass,
         typename KindTraits>
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

  // Adjusts the length of the fast backing store.
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
          isolate->heap()->RightTrimFixedArray<Heap::FROM_MUTATOR>(
              *backing_store, old_capacity - length);
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
    FastElementsAccessorSubclass::SetFastElementsCapacityAndLength(
        array, new_capacity, length);
    JSObject::ValidateElements(array);
    return length_object;
  }

  static Handle<Object> DeleteCommon(Handle<JSObject> obj, uint32_t key,
                                     LanguageMode language_mode) {
    DCHECK(obj->HasFastSmiOrObjectElements() ||
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
          BackingStore::cast(Handle<FixedArray>::cast(backing_store)->get(1)),
          isolate);
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

  virtual MaybeHandle<Object> Delete(Handle<JSObject> obj, uint32_t key,
                                     LanguageMode language_mode) FINAL {
    return DeleteCommon(obj, key, language_mode);
  }

  static bool HasElementImpl(
      Handle<JSObject> holder,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) {
    if (key >= static_cast<uint32_t>(backing_store->length())) {
      return false;
    }
    return !Handle<BackingStore>::cast(backing_store)->is_the_hole(key);
  }

  static void ValidateContents(Handle<JSObject> holder, int length) {
#if DEBUG
    Isolate* isolate = holder->GetIsolate();
    HandleScope scope(isolate);
    Handle<FixedArrayBase> elements(holder->elements(), isolate);
    Map* map = elements->map();
    DCHECK((IsFastSmiOrObjectElementsKind(KindTraits::Kind) &&
            (map == isolate->heap()->fixed_array_map() ||
             map == isolate->heap()->fixed_cow_array_map())) ||
           (IsFastDoubleElementsKind(KindTraits::Kind) ==
            ((map == isolate->heap()->fixed_array_map() && length == 0) ||
             map == isolate->heap()->fixed_double_array_map())));
    if (length == 0) return;  // nothing to do!
    DisallowHeapAllocation no_gc;
    Handle<BackingStore> backing_store = Handle<BackingStore>::cast(elements);
    for (int i = 0; i < length; i++) {
      DCHECK((!IsFastSmiElementsKind(KindTraits::Kind) ||
              BackingStore::get(backing_store, i)->IsSmi()) ||
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
    : public FastElementsAccessor<FastElementsAccessorSubclass, KindTraits> {
 public:
  explicit FastSmiOrObjectElementsAccessor(const char* name)
      : FastElementsAccessor<FastElementsAccessorSubclass,
                             KindTraits>(name) {}

  // NOTE: this method violates the handlified function signature convention:
  // raw pointer parameters in the function that allocates.
  // See ElementsAccessor::CopyElements() for details.
  // This method could actually allocate if copying from double elements to
  // object elements.
  static void CopyElementsImpl(FixedArrayBase* from, uint32_t from_start,
                               FixedArrayBase* to, ElementsKind from_kind,
                               uint32_t to_start, int packed_size,
                               int copy_size) {
    DisallowHeapAllocation no_gc;
    ElementsKind to_kind = KindTraits::Kind;
    switch (from_kind) {
      case FAST_SMI_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
        CopyObjectToObjectElements(from, from_kind, from_start, to, to_kind,
                                   to_start, copy_size);
        break;
      case FAST_DOUBLE_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS: {
        AllowHeapAllocation allow_allocation;
        CopyDoubleToObjectElements(
            from, from_start, to, to_kind, to_start, copy_size);
        break;
      }
      case DICTIONARY_ELEMENTS:
        CopyDictionaryToObjectElements(from, from_start, to, to_kind, to_start,
                                       copy_size);
        break;
      case SLOPPY_ARGUMENTS_ELEMENTS: {
        // TODO(verwaest): This is a temporary hack to support extending
        // SLOPPY_ARGUMENTS_ELEMENTS in SetFastElementsCapacityAndLength.
        // This case should be UNREACHABLE().
        FixedArray* parameter_map = FixedArray::cast(from);
        FixedArrayBase* arguments = FixedArrayBase::cast(parameter_map->get(1));
        ElementsKind from_kind = ElementsKindForArray(arguments);
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
    : public FastElementsAccessor<FastElementsAccessorSubclass, KindTraits> {
 public:
  explicit FastDoubleElementsAccessor(const char* name)
      : FastElementsAccessor<FastElementsAccessorSubclass,
                             KindTraits>(name) {}

  static void SetFastElementsCapacityAndLength(Handle<JSObject> obj,
                                               uint32_t capacity,
                                               uint32_t length) {
    JSObject::SetFastDoubleElementsCapacityAndLength(obj, capacity, length);
  }

 protected:
  static void CopyElementsImpl(FixedArrayBase* from, uint32_t from_start,
                               FixedArrayBase* to, ElementsKind from_kind,
                               uint32_t to_start, int packed_size,
                               int copy_size) {
    DisallowHeapAllocation no_allocation;
    switch (from_kind) {
      case FAST_SMI_ELEMENTS:
        CopyPackedSmiToDoubleElements(from, from_start, to, to_start,
                                      packed_size, copy_size);
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
        CopyDictionaryToDoubleElements(from, from_start, to, to_start,
                                       copy_size);
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

  MUST_USE_RESULT static MaybeHandle<Object> GetImpl(
      Handle<Object> receiver,
      Handle<JSObject> obj,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) {
    if (key < AccessorClass::GetCapacityImpl(backing_store)) {
      return BackingStore::get(Handle<BackingStore>::cast(backing_store), key);
    } else {
      return backing_store->GetIsolate()->factory()->undefined_value();
    }
  }

  MUST_USE_RESULT static PropertyAttributes GetAttributesImpl(
      Handle<JSObject> obj,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) {
    return
        key < AccessorClass::GetCapacityImpl(backing_store)
          ? NONE : ABSENT;
  }

  MUST_USE_RESULT static MaybeHandle<Object> SetLengthImpl(
      Handle<JSObject> obj,
      Handle<Object> length,
      Handle<FixedArrayBase> backing_store) {
    // External arrays do not support changing their length.
    UNREACHABLE();
    return obj;
  }

  MUST_USE_RESULT virtual MaybeHandle<Object> Delete(
      Handle<JSObject> obj, uint32_t key, LanguageMode language_mode) FINAL {
    // External arrays always ignore deletes.
    return obj->GetIsolate()->factory()->true_value();
  }

  static bool HasElementImpl(Handle<JSObject> holder, uint32_t key,
                             Handle<FixedArrayBase> backing_store) {
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
  static Handle<Object> SetLengthWithoutNormalize(
      Handle<FixedArrayBase> store,
      Handle<JSArray> array,
      Handle<Object> length_object,
      uint32_t length) {
    Handle<SeededNumberDictionary> dict =
        Handle<SeededNumberDictionary>::cast(store);
    Isolate* isolate = array->GetIsolate();
    int capacity = dict->Capacity();
    uint32_t new_length = length;
    uint32_t old_length = static_cast<uint32_t>(array->length()->Number());
    if (new_length < old_length) {
      // Find last non-deletable element in range of elements to be
      // deleted and adjust range accordingly.
      for (int i = 0; i < capacity; i++) {
        DisallowHeapAllocation no_gc;
        Object* key = dict->KeyAt(i);
        if (key->IsNumber()) {
          uint32_t number = static_cast<uint32_t>(key->Number());
          if (new_length <= number && number < old_length) {
            PropertyDetails details = dict->DetailsAt(i);
            if (!details.IsConfigurable()) new_length = number + 1;
          }
        }
      }
      if (new_length != length) {
        length_object = isolate->factory()->NewNumberFromUint(new_length);
      }
    }

    if (new_length == 0) {
      // Flush the backing store.
      JSObject::ResetElements(array);
    } else {
      DisallowHeapAllocation no_gc;
      // Remove elements that should be deleted.
      int removed_entries = 0;
      Handle<Object> the_hole_value = isolate->factory()->the_hole_value();
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

  MUST_USE_RESULT static MaybeHandle<Object> DeleteCommon(
      Handle<JSObject> obj, uint32_t key, LanguageMode language_mode) {
    Isolate* isolate = obj->GetIsolate();
    Handle<FixedArray> backing_store(FixedArray::cast(obj->elements()),
                                     isolate);
    bool is_arguments =
        (obj->GetElementsKind() == SLOPPY_ARGUMENTS_ELEMENTS);
    if (is_arguments) {
      backing_store = handle(FixedArray::cast(backing_store->get(1)), isolate);
    }
    Handle<SeededNumberDictionary> dictionary =
        Handle<SeededNumberDictionary>::cast(backing_store);
    int entry = dictionary->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound) {
      Handle<Object> result =
          SeededNumberDictionary::DeleteProperty(dictionary, entry);
      if (*result == *isolate->factory()->false_value()) {
        if (is_strict(language_mode)) {
          // Deleting a non-configurable property in strict mode.
          Handle<Object> name = isolate->factory()->NewNumberFromUint(key);
          Handle<Object> args[2] = { name, obj };
          THROW_NEW_ERROR(isolate, NewTypeError("strict_delete_property",
                                                HandleVector(args, 2)),
                          Object);
        }
        return isolate->factory()->false_value();
      }
      Handle<FixedArray> new_elements =
          SeededNumberDictionary::Shrink(dictionary, key);

      if (is_arguments) {
        FixedArray::cast(obj->elements())->set(1, *new_elements);
      } else {
        obj->set_elements(*new_elements);
      }
    }
    return isolate->factory()->true_value();
  }

  static void CopyElementsImpl(FixedArrayBase* from, uint32_t from_start,
                               FixedArrayBase* to, ElementsKind from_kind,
                               uint32_t to_start, int packed_size,
                               int copy_size) {
    UNREACHABLE();
  }


 protected:
  friend class ElementsAccessorBase<DictionaryElementsAccessor,
                                    ElementsKindTraits<DICTIONARY_ELEMENTS> >;

  MUST_USE_RESULT virtual MaybeHandle<Object> Delete(
      Handle<JSObject> obj, uint32_t key, LanguageMode language_mode) FINAL {
    return DeleteCommon(obj, key, language_mode);
  }

  MUST_USE_RESULT static MaybeHandle<Object> GetImpl(
      Handle<Object> receiver,
      Handle<JSObject> obj,
      uint32_t key,
      Handle<FixedArrayBase> store) {
    Handle<SeededNumberDictionary> backing_store =
        Handle<SeededNumberDictionary>::cast(store);
    Isolate* isolate = backing_store->GetIsolate();
    int entry = backing_store->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound) {
      Handle<Object> element(backing_store->ValueAt(entry), isolate);
      PropertyDetails details = backing_store->DetailsAt(entry);
      if (details.type() == ACCESSOR_CONSTANT) {
        return JSObject::GetElementWithCallback(
            obj, receiver, element, key, obj);
      } else {
        return element;
      }
    }
    return isolate->factory()->the_hole_value();
  }

  MUST_USE_RESULT static PropertyAttributes GetAttributesImpl(
      Handle<JSObject> obj,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) {
    Handle<SeededNumberDictionary> dictionary =
        Handle<SeededNumberDictionary>::cast(backing_store);
    int entry = dictionary->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound) {
      return dictionary->DetailsAt(entry).attributes();
    }
    return ABSENT;
  }

  MUST_USE_RESULT static MaybeHandle<AccessorPair> GetAccessorPairImpl(
      Handle<JSObject> obj,
      uint32_t key,
      Handle<FixedArrayBase> store) {
    Handle<SeededNumberDictionary> backing_store =
        Handle<SeededNumberDictionary>::cast(store);
    int entry = backing_store->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound &&
        backing_store->DetailsAt(entry).type() == ACCESSOR_CONSTANT &&
        backing_store->ValueAt(entry)->IsAccessorPair()) {
      return handle(AccessorPair::cast(backing_store->ValueAt(entry)));
    }
    return MaybeHandle<AccessorPair>();
  }

  static bool HasElementImpl(Handle<JSObject> holder, uint32_t key,
                             Handle<FixedArrayBase> store) {
    Handle<SeededNumberDictionary> backing_store =
        Handle<SeededNumberDictionary>::cast(store);
    return backing_store->FindEntry(key) != SeededNumberDictionary::kNotFound;
  }

  static uint32_t GetKeyForIndexImpl(Handle<FixedArrayBase> store,
                                     uint32_t index) {
    DisallowHeapAllocation no_gc;
    Handle<SeededNumberDictionary> dict =
        Handle<SeededNumberDictionary>::cast(store);
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

  MUST_USE_RESULT static MaybeHandle<Object> GetImpl(
      Handle<Object> receiver,
      Handle<JSObject> obj,
      uint32_t key,
      Handle<FixedArrayBase> parameters) {
    Isolate* isolate = obj->GetIsolate();
    Handle<FixedArray> parameter_map = Handle<FixedArray>::cast(parameters);
    Handle<Object> probe = GetParameterMapArg(obj, parameter_map, key);
    if (!probe->IsTheHole()) {
      DisallowHeapAllocation no_gc;
      Context* context = Context::cast(parameter_map->get(0));
      int context_index = Handle<Smi>::cast(probe)->value();
      DCHECK(!context->get(context_index)->IsTheHole());
      return handle(context->get(context_index), isolate);
    } else {
      // Object is not mapped, defer to the arguments.
      Handle<FixedArray> arguments(FixedArray::cast(parameter_map->get(1)),
                                   isolate);
      Handle<Object> result;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result,
          ElementsAccessor::ForArray(arguments)->Get(
              receiver, obj, key, arguments),
          Object);
      // Elements of the arguments object in slow mode might be slow aliases.
      if (result->IsAliasedArgumentsEntry()) {
        DisallowHeapAllocation no_gc;
        AliasedArgumentsEntry* entry = AliasedArgumentsEntry::cast(*result);
        Context* context = Context::cast(parameter_map->get(0));
        int context_index = entry->aliased_context_slot();
        DCHECK(!context->get(context_index)->IsTheHole());
        return handle(context->get(context_index), isolate);
      } else {
        return result;
      }
    }
  }

  MUST_USE_RESULT static PropertyAttributes GetAttributesImpl(
      Handle<JSObject> obj,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) {
    Handle<FixedArray> parameter_map = Handle<FixedArray>::cast(backing_store);
    Handle<Object> probe = GetParameterMapArg(obj, parameter_map, key);
    if (!probe->IsTheHole()) {
      return NONE;
    } else {
      // If not aliased, check the arguments.
      Handle<FixedArray> arguments(FixedArray::cast(parameter_map->get(1)));
      return ElementsAccessor::ForArray(arguments)
          ->GetAttributes(obj, key, arguments);
    }
  }

  MUST_USE_RESULT static MaybeHandle<AccessorPair> GetAccessorPairImpl(
      Handle<JSObject> obj,
      uint32_t key,
      Handle<FixedArrayBase> parameters) {
    Handle<FixedArray> parameter_map = Handle<FixedArray>::cast(parameters);
    Handle<Object> probe = GetParameterMapArg(obj, parameter_map, key);
    if (!probe->IsTheHole()) {
      return MaybeHandle<AccessorPair>();
    } else {
      // If not aliased, check the arguments.
      Handle<FixedArray> arguments(FixedArray::cast(parameter_map->get(1)));
      return ElementsAccessor::ForArray(arguments)
          ->GetAccessorPair(obj, key, arguments);
    }
  }

  MUST_USE_RESULT static MaybeHandle<Object> SetLengthImpl(
      Handle<JSObject> obj,
      Handle<Object> length,
      Handle<FixedArrayBase> parameter_map) {
    // TODO(mstarzinger): This was never implemented but will be used once we
    // correctly implement [[DefineOwnProperty]] on arrays.
    UNIMPLEMENTED();
    return obj;
  }

  MUST_USE_RESULT virtual MaybeHandle<Object> Delete(
      Handle<JSObject> obj, uint32_t key, LanguageMode language_mode) FINAL {
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
        return DictionaryElementsAccessor::DeleteCommon(obj, key,
                                                        language_mode);
      } else {
        // It's difficult to access the version of DeleteCommon that is declared
        // in the templatized super class, call the concrete implementation in
        // the class for the most generalized ElementsKind subclass.
        return FastHoleyObjectElementsAccessor::DeleteCommon(obj, key,
                                                             language_mode);
      }
    }
    return isolate->factory()->true_value();
  }

  static void CopyElementsImpl(FixedArrayBase* from, uint32_t from_start,
                               FixedArrayBase* to, ElementsKind from_kind,
                               uint32_t to_start, int packed_size,
                               int copy_size) {
    UNREACHABLE();
  }

  static uint32_t GetCapacityImpl(Handle<FixedArrayBase> backing_store) {
    Handle<FixedArray> parameter_map = Handle<FixedArray>::cast(backing_store);
    Handle<FixedArrayBase> arguments(
        FixedArrayBase::cast(parameter_map->get(1)));
    return Max(static_cast<uint32_t>(parameter_map->length() - 2),
               ForArray(arguments)->GetCapacity(arguments));
  }

  static uint32_t GetKeyForIndexImpl(Handle<FixedArrayBase> dict,
                                     uint32_t index) {
    return index;
  }

 private:
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


ElementsAccessor* ElementsAccessor::ForArray(Handle<FixedArrayBase> array) {
  return elements_accessors_[ElementsKindForArray(*array)];
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
  if (elements_accessors_ == NULL) return;
#define ACCESSOR_DELETE(Class, Kind, Store) delete elements_accessors_[Kind];
  ELEMENTS_LIST(ACCESSOR_DELETE)
#undef ACCESSOR_DELETE
  elements_accessors_ = NULL;
}


template <typename ElementsAccessorSubclass, typename ElementsKindTraits>
MUST_USE_RESULT
MaybeHandle<Object> ElementsAccessorBase<ElementsAccessorSubclass,
                                         ElementsKindTraits>::
    SetLengthImpl(Handle<JSObject> obj,
                  Handle<Object> length,
                  Handle<FixedArrayBase> backing_store) {
  Isolate* isolate = obj->GetIsolate();
  Handle<JSArray> array = Handle<JSArray>::cast(obj);

  // Fast case: The new length fits into a Smi.
  Handle<Object> smi_length;

  if (Object::ToSmi(isolate, length).ToHandle(&smi_length) &&
      smi_length->IsSmi()) {
    const int value = Handle<Smi>::cast(smi_length)->value();
    if (value >= 0) {
      Handle<Object> new_length = ElementsAccessorSubclass::
          SetLengthWithoutNormalize(backing_store, array, smi_length, value);
      DCHECK(!new_length.is_null());

      // even though the proposed length was a smi, new_length could
      // still be a heap number because SetLengthWithoutNormalize doesn't
      // allow the array length property to drop below the index of
      // non-deletable elements.
      DCHECK(new_length->IsSmi() || new_length->IsHeapNumber() ||
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
      DCHECK(!dictionary.is_null());

      Handle<Object> new_length = DictionaryElementsAccessor::
          SetLengthWithoutNormalize(dictionary, array, length, value);
      DCHECK(!new_length.is_null());

      DCHECK(new_length->IsNumber());
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


MaybeHandle<Object> ArrayConstructInitializeElements(Handle<JSArray> array,
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

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/conversions.h"
#include "src/elements.h"
#include "src/messages.h"
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
    if (key->KeyEquals(element)) return true;
  }
  return false;
}


MUST_USE_RESULT
static MaybeHandle<Object> ThrowArrayLengthRangeError(Isolate* isolate) {
  THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kInvalidArrayLength),
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

  static ElementsKind kind() { return ElementsTraits::Kind; }

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

  void Validate(Handle<JSObject> holder) final {
    DisallowHeapAllocation no_gc;
    ElementsAccessorSubclass::ValidateImpl(holder);
  }

  virtual bool HasElement(Handle<JSObject> holder, uint32_t key,
                          Handle<FixedArrayBase> backing_store) final {
    return ElementsAccessorSubclass::GetIndexForKeyImpl(*holder, *backing_store,
                                                        key) != kMaxUInt32;
  }

  virtual Handle<Object> Get(Handle<JSObject> holder, uint32_t key,
                             Handle<FixedArrayBase> backing_store) final {
    if (!IsExternalArrayElementsKind(ElementsTraits::Kind) &&
        FLAG_trace_js_array_abuse) {
      CheckArrayAbuse(holder, "elements read", key);
    }

    if (IsExternalArrayElementsKind(ElementsTraits::Kind) &&
        FLAG_trace_external_array_abuse) {
      CheckArrayAbuse(holder, "external elements read", key);
    }

    return ElementsAccessorSubclass::GetImpl(holder, key, backing_store);
  }

  static Handle<Object> GetImpl(Handle<JSObject> obj, uint32_t key,
                                Handle<FixedArrayBase> backing_store) {
    if (key < ElementsAccessorSubclass::GetCapacityImpl(*obj, *backing_store)) {
      return BackingStore::get(Handle<BackingStore>::cast(backing_store), key);
    } else {
      return backing_store->GetIsolate()->factory()->the_hole_value();
    }
  }

  virtual void Set(FixedArrayBase* backing_store, uint32_t key,
                   Object* value) final {
    ElementsAccessorSubclass::SetImpl(backing_store, key, value);
  }

  static void SetImpl(FixedArrayBase* backing_store, uint32_t key,
                      Object* value) {
    BackingStore::cast(backing_store)->SetValue(key, value);
  }

  virtual void Reconfigure(Handle<JSObject> object,
                           Handle<FixedArrayBase> store, uint32_t index,
                           Handle<Object> value,
                           PropertyAttributes attributes) final {
    ElementsAccessorSubclass::ReconfigureImpl(object, store, index, value,
                                              attributes);
  }

  static void ReconfigureImpl(Handle<JSObject> object,
                              Handle<FixedArrayBase> store, uint32_t index,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
    UNREACHABLE();
  }

  virtual void Add(Handle<JSObject> object, uint32_t index,
                   Handle<Object> value, PropertyAttributes attributes,
                   uint32_t new_capacity) final {
    ElementsAccessorSubclass::AddImpl(object, index, value, attributes,
                                      new_capacity);
  }

  static void AddImpl(Handle<JSObject> object, uint32_t index,
                      Handle<Object> value, PropertyAttributes attributes,
                      uint32_t new_capacity) {
    UNREACHABLE();
  }

  virtual MaybeHandle<AccessorPair> GetAccessorPair(
      Handle<JSObject> holder, uint32_t key,
      Handle<FixedArrayBase> backing_store) final {
    return ElementsAccessorSubclass::GetAccessorPairImpl(holder, key,
                                                         backing_store);
  }

  static MaybeHandle<AccessorPair> GetAccessorPairImpl(
      Handle<JSObject> obj, uint32_t key,
      Handle<FixedArrayBase> backing_store) {
    return MaybeHandle<AccessorPair>();
  }

  virtual void SetLength(Handle<JSArray> array, uint32_t length) final {
    ElementsAccessorSubclass::SetLengthImpl(array, length,
                                            handle(array->elements()));
  }

  static void SetLengthImpl(Handle<JSArray> array, uint32_t length,
                            Handle<FixedArrayBase> backing_store);

  static Handle<FixedArrayBase> ConvertElementsWithCapacity(
      Handle<JSObject> object, Handle<FixedArrayBase> old_elements,
      ElementsKind from_kind, uint32_t capacity) {
    Isolate* isolate = object->GetIsolate();
    Handle<FixedArrayBase> elements;
    if (IsFastDoubleElementsKind(kind())) {
      elements = isolate->factory()->NewFixedDoubleArray(capacity);
    } else {
      elements = isolate->factory()->NewUninitializedFixedArray(capacity);
    }

    int packed = kPackedSizeNotKnown;
    if (IsFastPackedElementsKind(from_kind) && object->IsJSArray()) {
      packed = Smi::cast(JSArray::cast(*object)->length())->value();
    }

    ElementsAccessorSubclass::CopyElementsImpl(
        *old_elements, 0, *elements, from_kind, 0, packed,
        ElementsAccessor::kCopyToEndAndInitializeToHole);
    return elements;
  }

  static void GrowCapacityAndConvertImpl(Handle<JSObject> object,
                                         uint32_t capacity) {
    ElementsKind from_kind = object->GetElementsKind();
    if (IsFastSmiOrObjectElementsKind(from_kind)) {
      // Array optimizations rely on the prototype lookups of Array objects
      // always returning undefined. If there is a store to the initial
      // prototype object, make sure all of these optimizations are invalidated.
      object->GetIsolate()->UpdateArrayProtectorOnSetLength(object);
    }
    Handle<FixedArrayBase> old_elements(object->elements());
    // This method should only be called if there's a reason to update the
    // elements.
    DCHECK(IsFastDoubleElementsKind(from_kind) !=
               IsFastDoubleElementsKind(kind()) ||
           IsDictionaryElementsKind(from_kind) ||
           static_cast<uint32_t>(old_elements->length()) < capacity);
    Handle<FixedArrayBase> elements =
        ConvertElementsWithCapacity(object, old_elements, from_kind, capacity);

    ElementsKind to_kind = kind();
    if (IsHoleyElementsKind(from_kind)) to_kind = GetHoleyElementsKind(to_kind);
    Handle<Map> new_map = JSObject::GetElementsTransitionMap(object, to_kind);
    JSObject::SetMapAndElements(object, new_map, elements);

    // Transition through the allocation site as well if present.
    JSObject::UpdateAllocationSite(object, to_kind);

    if (FLAG_trace_elements_transitions) {
      JSObject::PrintElementsTransition(stdout, object, from_kind, old_elements,
                                        to_kind, elements);
    }
  }

  virtual void GrowCapacityAndConvert(Handle<JSObject> object,
                                      uint32_t capacity) final {
    ElementsAccessorSubclass::GrowCapacityAndConvertImpl(object, capacity);
  }

  virtual void Delete(Handle<JSObject> obj, uint32_t key,
                      LanguageMode language_mode) override = 0;

  static void CopyElementsImpl(FixedArrayBase* from, uint32_t from_start,
                               FixedArrayBase* to, ElementsKind from_kind,
                               uint32_t to_start, int packed_size,
                               int copy_size) {
    UNREACHABLE();
  }

  virtual void CopyElements(Handle<FixedArrayBase> from, uint32_t from_start,
                            ElementsKind from_kind, Handle<FixedArrayBase> to,
                            uint32_t to_start, int copy_size) final {
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
                            uint32_t to_start, int copy_size) final {
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

  virtual Handle<FixedArray> AddElementsToFixedArray(
      Handle<JSObject> receiver, Handle<FixedArray> to,
      FixedArray::KeyFilter filter) final {
    Handle<FixedArrayBase> from(receiver->elements());

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
    uint32_t len1 = ElementsAccessorSubclass::GetCapacityImpl(*receiver, *from);
    if (len1 == 0) return to;

    Isolate* isolate = from->GetIsolate();

    // Compute how many elements are not in other.
    uint32_t extra = 0;
    for (uint32_t y = 0; y < len1; y++) {
      if (ElementsAccessorSubclass::HasIndexImpl(*from, y)) {
        uint32_t key = ElementsAccessorSubclass::GetKeyForIndexImpl(*from, y);
        Handle<Object> value =
            ElementsAccessorSubclass::GetImpl(receiver, key, from);

        DCHECK(!value->IsTheHole());
        DCHECK(!value->IsAccessorPair());
        DCHECK(!value->IsExecutableAccessorInfo());
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
      if (ElementsAccessorSubclass::HasIndexImpl(*from, y)) {
        uint32_t key = ElementsAccessorSubclass::GetKeyForIndexImpl(*from, y);
        Handle<Object> value =
            ElementsAccessorSubclass::GetImpl(receiver, key, from);
        DCHECK(!value->IsAccessorPair());
        DCHECK(!value->IsExecutableAccessorInfo());
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
  static uint32_t GetCapacityImpl(JSObject* holder,
                                  FixedArrayBase* backing_store) {
    return backing_store->length();
  }

  uint32_t GetCapacity(JSObject* holder, FixedArrayBase* backing_store) final {
    return ElementsAccessorSubclass::GetCapacityImpl(holder, backing_store);
  }

  static bool HasIndexImpl(FixedArrayBase* backing_store, uint32_t index) {
    return true;
  }

  virtual bool HasIndex(FixedArrayBase* backing_store, uint32_t index) final {
    return ElementsAccessorSubclass::HasIndexImpl(backing_store, index);
  }

  static uint32_t GetKeyForIndexImpl(FixedArrayBase* backing_store,
                                     uint32_t index) {
    return index;
  }

  virtual uint32_t GetKeyForIndex(FixedArrayBase* backing_store,
                                  uint32_t index) final {
    return ElementsAccessorSubclass::GetKeyForIndexImpl(backing_store, index);
  }

  static uint32_t GetIndexForKeyImpl(JSObject* holder,
                                     FixedArrayBase* backing_store,
                                     uint32_t key) {
    return key < ElementsAccessorSubclass::GetCapacityImpl(holder,
                                                           backing_store) &&
                   !BackingStore::cast(backing_store)->is_the_hole(key)
               ? key
               : kMaxUInt32;
  }

  virtual uint32_t GetIndexForKey(JSObject* holder,
                                  FixedArrayBase* backing_store,
                                  uint32_t key) final {
    return ElementsAccessorSubclass::GetIndexForKeyImpl(holder, backing_store,
                                                        key);
  }

  static PropertyDetails GetDetailsImpl(FixedArrayBase* backing_store,
                                        uint32_t index) {
    return PropertyDetails(NONE, DATA, 0, PropertyCellType::kNoCell);
  }

  virtual PropertyDetails GetDetails(FixedArrayBase* backing_store,
                                     uint32_t index) final {
    return ElementsAccessorSubclass::GetDetailsImpl(backing_store, index);
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

  static void DeleteCommon(Handle<JSObject> obj, uint32_t key,
                           LanguageMode language_mode) {
    DCHECK(obj->HasFastSmiOrObjectElements() ||
           obj->HasFastDoubleElements() ||
           obj->HasFastArgumentsElements());
    Isolate* isolate = obj->GetIsolate();
    Heap* heap = obj->GetHeap();
    Handle<FixedArrayBase> elements(obj->elements());
    if (*elements == heap->empty_fixed_array()) return;

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
  }

  static void ReconfigureImpl(Handle<JSObject> object,
                              Handle<FixedArrayBase> store, uint32_t index,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
    Handle<SeededNumberDictionary> dictionary =
        JSObject::NormalizeElements(object);
    index = dictionary->FindEntry(index);
    object->GetElementsAccessor()->Reconfigure(object, dictionary, index, value,
                                               attributes);
  }

  static void AddImpl(Handle<JSObject> object, uint32_t index,
                      Handle<Object> value, PropertyAttributes attributes,
                      uint32_t new_capacity) {
    DCHECK_EQ(NONE, attributes);
    ElementsKind from_kind = object->GetElementsKind();
    ElementsKind to_kind = FastElementsAccessorSubclass::kind();
    if (IsDictionaryElementsKind(from_kind) ||
        IsFastDoubleElementsKind(from_kind) !=
            IsFastDoubleElementsKind(to_kind) ||
        FastElementsAccessorSubclass::GetCapacityImpl(
            *object, object->elements()) != new_capacity) {
      FastElementsAccessorSubclass::GrowCapacityAndConvertImpl(object,
                                                               new_capacity);
    } else {
      if (from_kind != to_kind) {
        JSObject::TransitionElementsKind(object, to_kind);
      }
      if (IsFastSmiOrObjectElementsKind(from_kind)) {
        DCHECK(IsFastSmiOrObjectElementsKind(to_kind));
        JSObject::EnsureWritableFastElements(object);
      }
    }
    FastElementsAccessorSubclass::SetImpl(object->elements(), index, *value);
  }

  virtual void Delete(Handle<JSObject> obj, uint32_t key,
                      LanguageMode language_mode) final {
    DeleteCommon(obj, key, language_mode);
  }

  static bool HasIndexImpl(FixedArrayBase* backing_store, uint32_t index) {
    return !BackingStore::cast(backing_store)->is_the_hole(index);
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
    if (IsFastSmiElementsKind(KindTraits::Kind)) {
      for (int i = 0; i < length; i++) {
        DCHECK(BackingStore::get(backing_store, i)->IsSmi() ||
               (IsFastHoleyElementsKind(KindTraits::Kind) &&
                backing_store->is_the_hole(i)));
      }
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
        // SLOPPY_ARGUMENTS_ELEMENTS in GrowCapacityAndConvert.
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

  static Handle<Object> GetImpl(Handle<JSObject> obj, uint32_t key,
                                Handle<FixedArrayBase> backing_store) {
    if (key < AccessorClass::GetCapacityImpl(*obj, *backing_store)) {
      return BackingStore::get(Handle<BackingStore>::cast(backing_store), key);
    } else {
      return backing_store->GetIsolate()->factory()->undefined_value();
    }
  }

  static PropertyDetails GetDetailsImpl(FixedArrayBase* backing_store,
                                        uint32_t index) {
    return PropertyDetails(DONT_DELETE, DATA, 0, PropertyCellType::kNoCell);
  }

  static void SetLengthImpl(Handle<JSArray> array, uint32_t length,
                            Handle<FixedArrayBase> backing_store) {
    // External arrays do not support changing their length.
    UNREACHABLE();
  }

  virtual void Delete(Handle<JSObject> obj, uint32_t key,
                      LanguageMode language_mode) final {
    // External arrays always ignore deletes.
  }

  static uint32_t GetIndexForKeyImpl(JSObject* holder,
                                     FixedArrayBase* backing_store,
                                     uint32_t key) {
    return key < AccessorClass::GetCapacityImpl(holder, backing_store)
               ? key
               : kMaxUInt32;
  }

  static uint32_t GetCapacityImpl(JSObject* holder,
                                  FixedArrayBase* backing_store) {
    JSArrayBufferView* view = JSArrayBufferView::cast(holder);
    if (view->WasNeutered()) return 0;
    return backing_store->length();
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

  static void SetLengthImpl(Handle<JSArray> array, uint32_t length,
                            Handle<FixedArrayBase> backing_store) {
    Handle<SeededNumberDictionary> dict =
        Handle<SeededNumberDictionary>::cast(backing_store);
    Isolate* isolate = array->GetIsolate();
    int capacity = dict->Capacity();
    uint32_t old_length = 0;
    CHECK(array->length()->ToArrayLength(&old_length));
    if (length < old_length) {
      if (dict->requires_slow_elements()) {
        // Find last non-deletable element in range of elements to be
        // deleted and adjust range accordingly.
        for (int i = 0; i < capacity; i++) {
          DisallowHeapAllocation no_gc;
          Object* key = dict->KeyAt(i);
          if (key->IsNumber()) {
            uint32_t number = static_cast<uint32_t>(key->Number());
            if (length <= number && number < old_length) {
              PropertyDetails details = dict->DetailsAt(i);
              if (!details.IsConfigurable()) length = number + 1;
            }
          }
        }
      }

      if (length == 0) {
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
            if (length <= number && number < old_length) {
              dict->SetEntry(i, the_hole_value, the_hole_value);
              removed_entries++;
            }
          }
        }

        // Update the number of elements.
        dict->ElementsRemoved(removed_entries);
      }
    }

    Handle<Object> length_obj = isolate->factory()->NewNumberFromUint(length);
    array->set_length(*length_obj);
  }

  static void DeleteCommon(Handle<JSObject> obj, uint32_t key,
                           LanguageMode language_mode) {
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
      USE(result);
      DCHECK(result->IsTrue());
      Handle<FixedArray> new_elements =
          SeededNumberDictionary::Shrink(dictionary, key);

      if (is_arguments) {
        FixedArray::cast(obj->elements())->set(1, *new_elements);
      } else {
        obj->set_elements(*new_elements);
      }
    }
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

  virtual void Delete(Handle<JSObject> obj, uint32_t key,
                      LanguageMode language_mode) final {
    DeleteCommon(obj, key, language_mode);
  }

  static Handle<Object> GetImpl(Handle<JSObject> obj, uint32_t key,
                                Handle<FixedArrayBase> store) {
    Handle<SeededNumberDictionary> backing_store =
        Handle<SeededNumberDictionary>::cast(store);
    Isolate* isolate = backing_store->GetIsolate();
    int entry = backing_store->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound) {
      return handle(backing_store->ValueAt(entry), isolate);
    }
    return isolate->factory()->the_hole_value();
  }

  static void SetImpl(FixedArrayBase* store, uint32_t key, Object* value) {
    SeededNumberDictionary* dictionary = SeededNumberDictionary::cast(store);
    int entry = dictionary->FindEntry(key);
    DCHECK_NE(SeededNumberDictionary::kNotFound, entry);
    dictionary->ValueAtPut(entry, value);
  }

  static void ReconfigureImpl(Handle<JSObject> object,
                              Handle<FixedArrayBase> store, uint32_t index,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
    SeededNumberDictionary* dictionary = SeededNumberDictionary::cast(*store);
    if (attributes != NONE) dictionary->set_requires_slow_elements();
    dictionary->ValueAtPut(index, *value);
    PropertyDetails details = dictionary->DetailsAt(index);
    details = PropertyDetails(attributes, DATA, details.dictionary_index(),
                              PropertyCellType::kNoCell);
    dictionary->DetailsAtPut(index, details);
  }

  static void AddImpl(Handle<JSObject> object, uint32_t index,
                      Handle<Object> value, PropertyAttributes attributes,
                      uint32_t new_capacity) {
    PropertyDetails details(attributes, DATA, 0, PropertyCellType::kNoCell);
    Handle<SeededNumberDictionary> dictionary =
        object->HasFastElements()
            ? JSObject::NormalizeElements(object)
            : handle(SeededNumberDictionary::cast(object->elements()));
    Handle<SeededNumberDictionary> new_dictionary =
        SeededNumberDictionary::AddNumberEntry(dictionary, index, value,
                                               details);
    if (attributes != NONE) new_dictionary->set_requires_slow_elements();
    if (dictionary.is_identical_to(new_dictionary)) return;
    object->set_elements(*new_dictionary);
  }

  static MaybeHandle<AccessorPair> GetAccessorPairImpl(
      Handle<JSObject> obj, uint32_t key, Handle<FixedArrayBase> store) {
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

  static bool HasIndexImpl(FixedArrayBase* store, uint32_t index) {
    DisallowHeapAllocation no_gc;
    SeededNumberDictionary* dict = SeededNumberDictionary::cast(store);
    Object* key = dict->KeyAt(index);
    return !key->IsTheHole();
  }

  static uint32_t GetKeyForIndexImpl(FixedArrayBase* store, uint32_t index) {
    DisallowHeapAllocation no_gc;
    SeededNumberDictionary* dict = SeededNumberDictionary::cast(store);
    uint32_t result = 0;
    CHECK(dict->KeyAt(index)->ToArrayIndex(&result));
    return result;
  }

  static uint32_t GetIndexForKeyImpl(JSObject* holder, FixedArrayBase* store,
                                     uint32_t key) {
    DisallowHeapAllocation no_gc;
    SeededNumberDictionary* dict = SeededNumberDictionary::cast(store);
    int entry = dict->FindEntry(key);
    return entry == SeededNumberDictionary::kNotFound
               ? kMaxUInt32
               : static_cast<uint32_t>(entry);
  }

  static PropertyDetails GetDetailsImpl(FixedArrayBase* backing_store,
                                        uint32_t index) {
    return SeededNumberDictionary::cast(backing_store)->DetailsAt(index);
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

  static Handle<Object> GetImpl(Handle<JSObject> obj, uint32_t key,
                                Handle<FixedArrayBase> parameters) {
    Isolate* isolate = obj->GetIsolate();
    Handle<FixedArray> parameter_map = Handle<FixedArray>::cast(parameters);
    Handle<Object> probe(GetParameterMapArg(*parameter_map, key), isolate);
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
      Handle<Object> result =
          ElementsAccessor::ForArray(arguments)->Get(obj, key, arguments);
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

  static void GrowCapacityAndConvertImpl(Handle<JSObject> object,
                                         uint32_t capacity) {
    Handle<FixedArray> parameter_map(FixedArray::cast(object->elements()));
    Handle<FixedArray> old_elements(FixedArray::cast(parameter_map->get(1)));
    ElementsKind from_kind = old_elements->IsDictionary() ? DICTIONARY_ELEMENTS
                                                          : FAST_HOLEY_ELEMENTS;
    // This method should only be called if there's a reason to update the
    // elements.
    DCHECK(IsDictionaryElementsKind(from_kind) ||
           static_cast<uint32_t>(old_elements->length()) < capacity);
    Handle<FixedArrayBase> elements =
        ConvertElementsWithCapacity(object, old_elements, from_kind, capacity);
    parameter_map->set(1, *elements);
    JSObject::ValidateElements(object);
  }

  static void SetImpl(FixedArrayBase* store, uint32_t key, Object* value) {
    FixedArray* parameter_map = FixedArray::cast(store);
    Object* probe = GetParameterMapArg(parameter_map, key);
    if (!probe->IsTheHole()) {
      Context* context = Context::cast(parameter_map->get(0));
      int context_index = Smi::cast(probe)->value();
      DCHECK(!context->get(context_index)->IsTheHole());
      context->set(context_index, value);
    } else {
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      ElementsAccessor::ForArray(arguments)->Set(arguments, key, value);
    }
  }

  static void ReconfigureImpl(Handle<JSObject> object,
                              Handle<FixedArrayBase> store, uint32_t index,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
    Handle<FixedArray> parameter_map = Handle<FixedArray>::cast(store);
    uint32_t length = parameter_map->length() - 2;
    if (index < length) {
      Object* probe = parameter_map->get(index + 2);
      DCHECK(!probe->IsTheHole());
      Context* context = Context::cast(parameter_map->get(0));
      int context_index = Smi::cast(probe)->value();
      DCHECK(!context->get(context_index)->IsTheHole());
      context->set(context_index, *value);

      // Redefining attributes of an aliased element destroys fast aliasing.
      parameter_map->set_the_hole(index + 2);
      // For elements that are still writable we re-establish slow aliasing.
      if ((attributes & READ_ONLY) == 0) {
        Isolate* isolate = store->GetIsolate();
        value = isolate->factory()->NewAliasedArgumentsEntry(context_index);
      }

      PropertyDetails details(attributes, DATA, 0, PropertyCellType::kNoCell);
      Handle<SeededNumberDictionary> arguments =
          parameter_map->get(1)->IsSeededNumberDictionary()
              ? handle(SeededNumberDictionary::cast(parameter_map->get(1)))
              : JSObject::NormalizeElements(object);
      arguments = SeededNumberDictionary::AddNumberEntry(arguments, index,
                                                         value, details);
      parameter_map->set(1, *arguments);
    } else {
      Handle<FixedArrayBase> arguments(
          FixedArrayBase::cast(parameter_map->get(1)));
      ElementsAccessor::ForArray(arguments)
          ->Reconfigure(object, arguments, index - length, value, attributes);
    }
  }

  static void AddImpl(Handle<JSObject> object, uint32_t key,
                      Handle<Object> value, PropertyAttributes attributes,
                      uint32_t new_capacity) {
    DCHECK_EQ(NONE, attributes);
    Handle<FixedArray> parameter_map(FixedArray::cast(object->elements()));
    Handle<FixedArrayBase> old_elements(
        FixedArrayBase::cast(parameter_map->get(1)));
    if (old_elements->IsSeededNumberDictionary() ||
        static_cast<uint32_t>(old_elements->length()) < new_capacity) {
      GrowCapacityAndConvertImpl(object, new_capacity);
    }
    SetImpl(object->elements(), key, *value);
  }

  static MaybeHandle<AccessorPair> GetAccessorPairImpl(
      Handle<JSObject> obj, uint32_t key, Handle<FixedArrayBase> parameters) {
    Handle<FixedArray> parameter_map = Handle<FixedArray>::cast(parameters);
    Handle<Object> probe(GetParameterMapArg(*parameter_map, key),
                         obj->GetIsolate());
    if (!probe->IsTheHole()) {
      return MaybeHandle<AccessorPair>();
    } else {
      // If not aliased, check the arguments.
      Handle<FixedArray> arguments(FixedArray::cast(parameter_map->get(1)));
      return ElementsAccessor::ForArray(arguments)
          ->GetAccessorPair(obj, key, arguments);
    }
  }

  static void SetLengthImpl(Handle<JSArray> array, uint32_t length,
                            Handle<FixedArrayBase> parameter_map) {
    // Sloppy arguments objects are not arrays.
    UNREACHABLE();
  }

  virtual void Delete(Handle<JSObject> obj, uint32_t key,
                      LanguageMode language_mode) final {
    Isolate* isolate = obj->GetIsolate();
    Handle<FixedArray> parameter_map(FixedArray::cast(obj->elements()));
    Handle<Object> probe(GetParameterMapArg(*parameter_map, key), isolate);
    if (!probe->IsTheHole()) {
      // TODO(kmillikin): We could check if this was the last aliased
      // parameter, and revert to normal elements in that case.  That
      // would enable GC of the context.
      parameter_map->set_the_hole(key + 2);
    } else {
      Handle<FixedArray> arguments(FixedArray::cast(parameter_map->get(1)));
      if (arguments->IsDictionary()) {
        DictionaryElementsAccessor::DeleteCommon(obj, key, language_mode);
      } else {
        // It's difficult to access the version of DeleteCommon that is declared
        // in the templatized super class, call the concrete implementation in
        // the class for the most generalized ElementsKind subclass.
        FastHoleyObjectElementsAccessor::DeleteCommon(obj, key, language_mode);
      }
    }
  }

  static void CopyElementsImpl(FixedArrayBase* from, uint32_t from_start,
                               FixedArrayBase* to, ElementsKind from_kind,
                               uint32_t to_start, int packed_size,
                               int copy_size) {
    DCHECK(!to->IsDictionary());
    if (from_kind == DICTIONARY_ELEMENTS) {
      CopyDictionaryToObjectElements(from, from_start, to, FAST_HOLEY_ELEMENTS,
                                     to_start, copy_size);
    } else {
      DCHECK_EQ(FAST_HOLEY_ELEMENTS, from_kind);
      CopyObjectToObjectElements(from, from_kind, from_start, to,
                                 FAST_HOLEY_ELEMENTS, to_start, copy_size);
    }
  }

  static uint32_t GetCapacityImpl(JSObject* holder,
                                  FixedArrayBase* backing_store) {
    FixedArray* parameter_map = FixedArray::cast(backing_store);
    FixedArrayBase* arguments = FixedArrayBase::cast(parameter_map->get(1));
    return parameter_map->length() - 2 +
           ForArray(arguments)->GetCapacity(holder, arguments);
  }

  static bool HasIndexImpl(FixedArrayBase* parameters, uint32_t index) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    uint32_t length = parameter_map->length() - 2;
    if (index < length) {
      return !GetParameterMapArg(parameter_map, index)->IsTheHole();
    }

    FixedArrayBase* arguments = FixedArrayBase::cast(parameter_map->get(1));
    return ForArray(arguments)->HasIndex(arguments, index - length);
  }

  static uint32_t GetKeyForIndexImpl(FixedArrayBase* parameters,
                                     uint32_t index) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    uint32_t length = parameter_map->length() - 2;
    if (index < length) return index;

    FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
    return ForArray(arguments)->GetKeyForIndex(arguments, index - length);
  }

  static uint32_t GetIndexForKeyImpl(JSObject* holder,
                                     FixedArrayBase* parameters, uint32_t key) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    Object* probe = GetParameterMapArg(parameter_map, key);
    if (!probe->IsTheHole()) return key;

    FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
    uint32_t index = ElementsAccessor::ForArray(arguments)
                         ->GetIndexForKey(holder, arguments, key);
    if (index == kMaxUInt32) return index;
    return (parameter_map->length() - 2) + index;
  }

  static PropertyDetails GetDetailsImpl(FixedArrayBase* parameters,
                                        uint32_t index) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    uint32_t length = parameter_map->length() - 2;
    if (index < length) {
      return PropertyDetails(NONE, DATA, 0, PropertyCellType::kNoCell);
    }
    index -= length;
    FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
    return ElementsAccessor::ForArray(arguments)->GetDetails(arguments, index);
  }

 private:
  static Object* GetParameterMapArg(FixedArray* parameter_map, uint32_t key) {
    uint32_t length = parameter_map->length() - 2;
    return key < length
               ? parameter_map->get(key + 2)
               : Object::cast(parameter_map->GetHeap()->the_hole_value());
  }
};


ElementsAccessor* ElementsAccessor::ForArray(FixedArrayBase* array) {
  return elements_accessors_[ElementsKindForArray(array)];
}


ElementsAccessor* ElementsAccessor::ForArray(Handle<FixedArrayBase> array) {
  return ForArray(*array);
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
void ElementsAccessorBase<ElementsAccessorSubclass, ElementsKindTraits>::
    SetLengthImpl(Handle<JSArray> array, uint32_t length,
                  Handle<FixedArrayBase> backing_store) {
  DCHECK(!array->SetLengthWouldNormalize(length));
  DCHECK(IsFastElementsKind(array->GetElementsKind()));
  uint32_t old_length = 0;
  CHECK(array->length()->ToArrayIndex(&old_length));

  if (old_length < length) {
    ElementsKind kind = array->GetElementsKind();
    if (!IsFastHoleyElementsKind(kind)) {
      kind = GetHoleyElementsKind(kind);
      JSObject::TransitionElementsKind(array, kind);
    }
  }

  // Check whether the backing store should be shrunk.
  uint32_t capacity = backing_store->length();
  if (length == 0) {
    array->initialize_elements();
  } else if (length <= capacity) {
    if (array->HasFastSmiOrObjectElements()) {
      backing_store = JSObject::EnsureWritableFastElements(array);
    }
    if (2 * length <= capacity) {
      // If more than half the elements won't be used, trim the array.
      array->GetHeap()->RightTrimFixedArray<Heap::CONCURRENT_TO_SWEEPER>(
          *backing_store, capacity - length);
    } else {
      // Otherwise, fill the unused tail with holes.
      for (uint32_t i = length; i < old_length; i++) {
        BackingStore::cast(*backing_store)->set_the_hole(i);
      }
    }
  } else {
    // Check whether the backing store should be expanded.
    capacity = Max(length, JSObject::NewElementsCapacity(capacity));
    ElementsAccessorSubclass::GrowCapacityAndConvertImpl(array, capacity);
  }

  array->set_length(Smi::FromInt(length));
  JSObject::ValidateElements(array);
}


MaybeHandle<Object> ArrayConstructInitializeElements(Handle<JSArray> array,
                                                     Arguments* args) {
  if (args->length() == 0) {
    // Optimize the case where there are no parameters passed.
    JSArray::Initialize(array, JSArray::kPreallocatedArrayElements);
    return array;

  } else if (args->length() == 1 && args->at<Object>(0)->IsNumber()) {
    uint32_t length;
    if (!args->at<Object>(0)->ToArrayLength(&length)) {
      return ThrowArrayLengthRangeError(array->GetIsolate());
    }

    // Optimize the case where there is one argument and the argument is a small
    // smi.
    if (length > 0 && length < JSObject::kInitialMaxFastElementArray) {
      ElementsKind elements_kind = array->GetElementsKind();
      JSArray::Initialize(array, length, length);

      if (!IsFastHoleyElementsKind(elements_kind)) {
        elements_kind = GetHoleyElementsKind(elements_kind);
        JSObject::TransitionElementsKind(array, elements_kind);
      }
    } else if (length == 0) {
      JSArray::Initialize(array, JSArray::kPreallocatedArrayElements);
    } else {
      // Take the argument as the length.
      JSArray::Initialize(array, 0);
      JSArray::SetLength(array, length);
    }
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

}  // namespace internal
}  // namespace v8

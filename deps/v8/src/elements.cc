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
//     - FastSloppyArgumentsElementsAccessor
//     - SlowSloppyArgumentsElementsAccessor


namespace v8 {
namespace internal {


namespace {


static const int kPackedSizeNotKnown = -1;


// First argument in list is the accessor class, the second argument is the
// accessor ElementsKind, and the third is the backing store class.  Use the
// fast element handler for smi-only arrays.  The implementation is currently
// identical.  Note that the order must match that of the ElementsKind enum for
// the |accessor_array[]| below to work.
#define ELEMENTS_LIST(V)                                                      \
  V(FastPackedSmiElementsAccessor, FAST_SMI_ELEMENTS, FixedArray)             \
  V(FastHoleySmiElementsAccessor, FAST_HOLEY_SMI_ELEMENTS, FixedArray)        \
  V(FastPackedObjectElementsAccessor, FAST_ELEMENTS, FixedArray)              \
  V(FastHoleyObjectElementsAccessor, FAST_HOLEY_ELEMENTS, FixedArray)         \
  V(FastPackedDoubleElementsAccessor, FAST_DOUBLE_ELEMENTS, FixedDoubleArray) \
  V(FastHoleyDoubleElementsAccessor, FAST_HOLEY_DOUBLE_ELEMENTS,              \
    FixedDoubleArray)                                                         \
  V(DictionaryElementsAccessor, DICTIONARY_ELEMENTS, SeededNumberDictionary)  \
  V(FastSloppyArgumentsElementsAccessor, FAST_SLOPPY_ARGUMENTS_ELEMENTS,      \
    FixedArray)                                                               \
  V(SlowSloppyArgumentsElementsAccessor, SLOW_SLOPPY_ARGUMENTS_ELEMENTS,      \
    FixedArray)                                                               \
  V(ExternalInt8ElementsAccessor, EXTERNAL_INT8_ELEMENTS, ExternalInt8Array)  \
  V(ExternalUint8ElementsAccessor, EXTERNAL_UINT8_ELEMENTS,                   \
    ExternalUint8Array)                                                       \
  V(ExternalInt16ElementsAccessor, EXTERNAL_INT16_ELEMENTS,                   \
    ExternalInt16Array)                                                       \
  V(ExternalUint16ElementsAccessor, EXTERNAL_UINT16_ELEMENTS,                 \
    ExternalUint16Array)                                                      \
  V(ExternalInt32ElementsAccessor, EXTERNAL_INT32_ELEMENTS,                   \
    ExternalInt32Array)                                                       \
  V(ExternalUint32ElementsAccessor, EXTERNAL_UINT32_ELEMENTS,                 \
    ExternalUint32Array)                                                      \
  V(ExternalFloat32ElementsAccessor, EXTERNAL_FLOAT32_ELEMENTS,               \
    ExternalFloat32Array)                                                     \
  V(ExternalFloat64ElementsAccessor, EXTERNAL_FLOAT64_ELEMENTS,               \
    ExternalFloat64Array)                                                     \
  V(ExternalUint8ClampedElementsAccessor, EXTERNAL_UINT8_CLAMPED_ELEMENTS,    \
    ExternalUint8ClampedArray)                                                \
  V(FixedUint8ElementsAccessor, UINT8_ELEMENTS, FixedUint8Array)              \
  V(FixedInt8ElementsAccessor, INT8_ELEMENTS, FixedInt8Array)                 \
  V(FixedUint16ElementsAccessor, UINT16_ELEMENTS, FixedUint16Array)           \
  V(FixedInt16ElementsAccessor, INT16_ELEMENTS, FixedInt16Array)              \
  V(FixedUint32ElementsAccessor, UINT32_ELEMENTS, FixedUint32Array)           \
  V(FixedInt32ElementsAccessor, INT32_ELEMENTS, FixedInt32Array)              \
  V(FixedFloat32ElementsAccessor, FLOAT32_ELEMENTS, FixedFloat32Array)        \
  V(FixedFloat64ElementsAccessor, FLOAT64_ELEMENTS, FixedFloat64Array)        \
  V(FixedUint8ClampedElementsAccessor, UINT8_CLAMPED_ELEMENTS,                \
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


static bool HasIndex(Handle<FixedArray> array, Handle<Object> index_handle) {
  DisallowHeapAllocation no_gc;
  Object* index = *index_handle;
  int len0 = array->length();
  for (int i = 0; i < len0; i++) {
    Object* element = array->get(i);
    if (index->KeyEquals(element)) return true;
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
 public:
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

  virtual bool HasElement(Handle<JSObject> holder, uint32_t index,
                          Handle<FixedArrayBase> backing_store) final {
    return ElementsAccessorSubclass::GetEntryForIndexImpl(
               *holder, *backing_store, index) != kMaxUInt32;
  }

  virtual Handle<Object> Get(Handle<JSObject> holder, uint32_t index,
                             Handle<FixedArrayBase> backing_store) final {
    if (!IsExternalArrayElementsKind(ElementsTraits::Kind) &&
        FLAG_trace_js_array_abuse) {
      CheckArrayAbuse(holder, "elements read", index);
    }

    if (IsExternalArrayElementsKind(ElementsTraits::Kind) &&
        FLAG_trace_external_array_abuse) {
      CheckArrayAbuse(holder, "external elements read", index);
    }

    return ElementsAccessorSubclass::GetImpl(holder, index, backing_store);
  }

  static Handle<Object> GetImpl(Handle<JSObject> obj, uint32_t index,
                                Handle<FixedArrayBase> backing_store) {
    if (index <
        ElementsAccessorSubclass::GetCapacityImpl(*obj, *backing_store)) {
      return BackingStore::get(Handle<BackingStore>::cast(backing_store),
                               index);
    } else {
      return backing_store->GetIsolate()->factory()->the_hole_value();
    }
  }

  virtual void Set(FixedArrayBase* backing_store, uint32_t index,
                   Object* value) final {
    ElementsAccessorSubclass::SetImpl(backing_store, index, value);
  }

  static void SetImpl(FixedArrayBase* backing_store, uint32_t index,
                      Object* value) {
    BackingStore::cast(backing_store)->SetValue(index, value);
  }

  virtual void Reconfigure(Handle<JSObject> object,
                           Handle<FixedArrayBase> store, uint32_t entry,
                           Handle<Object> value,
                           PropertyAttributes attributes) final {
    ElementsAccessorSubclass::ReconfigureImpl(object, store, entry, value,
                                              attributes);
  }

  static void ReconfigureImpl(Handle<JSObject> object,
                              Handle<FixedArrayBase> store, uint32_t entry,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
    UNREACHABLE();
  }

  virtual void Add(Handle<JSObject> object, uint32_t entry,
                   Handle<Object> value, PropertyAttributes attributes,
                   uint32_t new_capacity) final {
    ElementsAccessorSubclass::AddImpl(object, entry, value, attributes,
                                      new_capacity);
  }

  static void AddImpl(Handle<JSObject> object, uint32_t entry,
                      Handle<Object> value, PropertyAttributes attributes,
                      uint32_t new_capacity) {
    UNREACHABLE();
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

  virtual void Delete(Handle<JSObject> obj, uint32_t entry) final {
    ElementsAccessorSubclass::DeleteImpl(obj, entry);
  }

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
      if (ElementsAccessorSubclass::HasEntryImpl(*from, y)) {
        uint32_t index =
            ElementsAccessorSubclass::GetIndexForEntryImpl(*from, y);
        Handle<Object> value =
            ElementsAccessorSubclass::GetImpl(receiver, index, from);

        DCHECK(!value->IsTheHole());
        DCHECK(!value->IsAccessorPair());
        DCHECK(!value->IsExecutableAccessorInfo());
        if (filter == FixedArray::NON_SYMBOL_KEYS && value->IsSymbol()) {
          continue;
        }
        if (!HasIndex(to, value)) {
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
    uint32_t entry = 0;
    for (uint32_t y = 0; y < len1; y++) {
      if (ElementsAccessorSubclass::HasEntryImpl(*from, y)) {
        uint32_t index =
            ElementsAccessorSubclass::GetIndexForEntryImpl(*from, y);
        Handle<Object> value =
            ElementsAccessorSubclass::GetImpl(receiver, index, from);
        DCHECK(!value->IsAccessorPair());
        DCHECK(!value->IsExecutableAccessorInfo());
        if (filter == FixedArray::NON_SYMBOL_KEYS && value->IsSymbol()) {
          continue;
        }
        if (!value->IsTheHole() && !HasIndex(to, value)) {
          result->set(len0 + entry, *value);
          entry++;
        }
      }
    }
    DCHECK(extra == entry);
    return result;
  }

  static uint32_t GetCapacityImpl(JSObject* holder,
                                  FixedArrayBase* backing_store) {
    return backing_store->length();
  }

  uint32_t GetCapacity(JSObject* holder, FixedArrayBase* backing_store) final {
    return ElementsAccessorSubclass::GetCapacityImpl(holder, backing_store);
  }

  static bool HasEntryImpl(FixedArrayBase* backing_store, uint32_t entry) {
    return true;
  }

  static uint32_t GetIndexForEntryImpl(FixedArrayBase* backing_store,
                                       uint32_t entry) {
    return entry;
  }

  static uint32_t GetEntryForIndexImpl(JSObject* holder,
                                       FixedArrayBase* backing_store,
                                       uint32_t index) {
    return index < ElementsAccessorSubclass::GetCapacityImpl(holder,
                                                             backing_store) &&
                   !BackingStore::cast(backing_store)->is_the_hole(index)
               ? index
               : kMaxUInt32;
  }

  virtual uint32_t GetEntryForIndex(JSObject* holder,
                                    FixedArrayBase* backing_store,
                                    uint32_t index) final {
    return ElementsAccessorSubclass::GetEntryForIndexImpl(holder, backing_store,
                                                          index);
  }

  static PropertyDetails GetDetailsImpl(FixedArrayBase* backing_store,
                                        uint32_t entry) {
    return PropertyDetails(NONE, DATA, 0, PropertyCellType::kNoCell);
  }

  virtual PropertyDetails GetDetails(FixedArrayBase* backing_store,
                                     uint32_t entry) final {
    return ElementsAccessorSubclass::GetDetailsImpl(backing_store, entry);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementsAccessorBase);
};


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
        for (int entry = 0; entry < capacity; entry++) {
          DisallowHeapAllocation no_gc;
          Object* index = dict->KeyAt(entry);
          if (index->IsNumber()) {
            uint32_t number = static_cast<uint32_t>(index->Number());
            if (length <= number && number < old_length) {
              PropertyDetails details = dict->DetailsAt(entry);
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
        for (int entry = 0; entry < capacity; entry++) {
          Object* index = dict->KeyAt(entry);
          if (index->IsNumber()) {
            uint32_t number = static_cast<uint32_t>(index->Number());
            if (length <= number && number < old_length) {
              dict->SetEntry(entry, the_hole_value, the_hole_value);
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

  static void CopyElementsImpl(FixedArrayBase* from, uint32_t from_start,
                               FixedArrayBase* to, ElementsKind from_kind,
                               uint32_t to_start, int packed_size,
                               int copy_size) {
    UNREACHABLE();
  }


  static void DeleteImpl(Handle<JSObject> obj, uint32_t entry) {
    // TODO(verwaest): Remove reliance on index in Shrink.
    Handle<SeededNumberDictionary> dict(
        SeededNumberDictionary::cast(obj->elements()));
    uint32_t index = GetIndexForEntryImpl(*dict, entry);
    Handle<Object> result = SeededNumberDictionary::DeleteProperty(dict, entry);
    USE(result);
    DCHECK(result->IsTrue());
    Handle<FixedArray> new_elements =
        SeededNumberDictionary::Shrink(dict, index);
    obj->set_elements(*new_elements);
  }

  static Handle<Object> GetImpl(Handle<JSObject> obj, uint32_t index,
                                Handle<FixedArrayBase> store) {
    Handle<SeededNumberDictionary> backing_store =
        Handle<SeededNumberDictionary>::cast(store);
    Isolate* isolate = backing_store->GetIsolate();
    int entry = backing_store->FindEntry(index);
    if (entry != SeededNumberDictionary::kNotFound) {
      return handle(backing_store->ValueAt(entry), isolate);
    }
    return isolate->factory()->the_hole_value();
  }

  static void SetImpl(FixedArrayBase* store, uint32_t index, Object* value) {
    SeededNumberDictionary* dictionary = SeededNumberDictionary::cast(store);
    int entry = dictionary->FindEntry(index);
    DCHECK_NE(SeededNumberDictionary::kNotFound, entry);
    dictionary->ValueAtPut(entry, value);
  }

  static void ReconfigureImpl(Handle<JSObject> object,
                              Handle<FixedArrayBase> store, uint32_t entry,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
    SeededNumberDictionary* dictionary = SeededNumberDictionary::cast(*store);
    if (attributes != NONE) dictionary->set_requires_slow_elements();
    dictionary->ValueAtPut(entry, *value);
    PropertyDetails details = dictionary->DetailsAt(entry);
    details = PropertyDetails(attributes, DATA, details.dictionary_index(),
                              PropertyCellType::kNoCell);
    dictionary->DetailsAtPut(entry, details);
  }

  static void AddImpl(Handle<JSObject> object, uint32_t entry,
                      Handle<Object> value, PropertyAttributes attributes,
                      uint32_t new_capacity) {
    PropertyDetails details(attributes, DATA, 0, PropertyCellType::kNoCell);
    Handle<SeededNumberDictionary> dictionary =
        object->HasFastElements()
            ? JSObject::NormalizeElements(object)
            : handle(SeededNumberDictionary::cast(object->elements()));
    Handle<SeededNumberDictionary> new_dictionary =
        SeededNumberDictionary::AddNumberEntry(dictionary, entry, value,
                                               details);
    if (attributes != NONE) new_dictionary->set_requires_slow_elements();
    if (dictionary.is_identical_to(new_dictionary)) return;
    object->set_elements(*new_dictionary);
  }

  static bool HasEntryImpl(FixedArrayBase* store, uint32_t entry) {
    DisallowHeapAllocation no_gc;
    SeededNumberDictionary* dict = SeededNumberDictionary::cast(store);
    Object* index = dict->KeyAt(entry);
    return !index->IsTheHole();
  }

  static uint32_t GetIndexForEntryImpl(FixedArrayBase* store, uint32_t entry) {
    DisallowHeapAllocation no_gc;
    SeededNumberDictionary* dict = SeededNumberDictionary::cast(store);
    uint32_t result = 0;
    CHECK(dict->KeyAt(entry)->ToArrayIndex(&result));
    return result;
  }

  static uint32_t GetEntryForIndexImpl(JSObject* holder, FixedArrayBase* store,
                                       uint32_t index) {
    DisallowHeapAllocation no_gc;
    SeededNumberDictionary* dict = SeededNumberDictionary::cast(store);
    int entry = dict->FindEntry(index);
    return entry == SeededNumberDictionary::kNotFound
               ? kMaxUInt32
               : static_cast<uint32_t>(entry);
  }

  static PropertyDetails GetDetailsImpl(FixedArrayBase* backing_store,
                                        uint32_t entry) {
    return SeededNumberDictionary::cast(backing_store)->DetailsAt(entry);
  }
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

  typedef typename KindTraits::BackingStore BackingStore;

  static void DeleteAtEnd(Handle<JSObject> obj,
                          Handle<BackingStore> backing_store, uint32_t entry) {
    uint32_t length = static_cast<uint32_t>(backing_store->length());
    Heap* heap = obj->GetHeap();
    for (; entry > 0; entry--) {
      if (!backing_store->is_the_hole(entry - 1)) break;
    }
    if (entry == 0) {
      FixedArray* empty = heap->empty_fixed_array();
      if (obj->HasFastArgumentsElements()) {
        FixedArray::cast(obj->elements())->set(1, empty);
      } else {
        obj->set_elements(empty);
      }
      return;
    }

    heap->RightTrimFixedArray<Heap::CONCURRENT_TO_SWEEPER>(*backing_store,
                                                           length - entry);
  }

  static void DeleteCommon(Handle<JSObject> obj, uint32_t entry,
                           Handle<FixedArrayBase> store) {
    DCHECK(obj->HasFastSmiOrObjectElements() ||
           obj->HasFastDoubleElements() ||
           obj->HasFastArgumentsElements());
    Handle<BackingStore> backing_store = Handle<BackingStore>::cast(store);
    if (!obj->IsJSArray() &&
        entry == static_cast<uint32_t>(store->length()) - 1) {
      DeleteAtEnd(obj, backing_store, entry);
      return;
    }

    backing_store->set_the_hole(entry);

    // TODO(verwaest): Move this out of elements.cc.
    // If an old space backing store is larger than a certain size and
    // has too few used values, normalize it.
    // To avoid doing the check on every delete we require at least
    // one adjacent hole to the value being deleted.
    const int kMinLengthForSparsenessCheck = 64;
    if (backing_store->length() < kMinLengthForSparsenessCheck) return;
    if (backing_store->GetHeap()->InNewSpace(*backing_store)) return;
    uint32_t length = 0;
    if (obj->IsJSArray()) {
      JSArray::cast(*obj)->length()->ToArrayLength(&length);
    } else {
      length = static_cast<uint32_t>(store->length());
    }
    if ((entry > 0 && backing_store->is_the_hole(entry - 1)) ||
        (entry + 1 < length && backing_store->is_the_hole(entry + 1))) {
      if (!obj->IsJSArray()) {
        uint32_t i;
        for (i = entry + 1; i < length; i++) {
          if (!backing_store->is_the_hole(i)) break;
        }
        if (i == length) {
          DeleteAtEnd(obj, backing_store, entry);
          return;
        }
      }
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

  static void ReconfigureImpl(Handle<JSObject> object,
                              Handle<FixedArrayBase> store, uint32_t entry,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
    Handle<SeededNumberDictionary> dictionary =
        JSObject::NormalizeElements(object);
    entry = dictionary->FindEntry(entry);
    DictionaryElementsAccessor::ReconfigureImpl(object, dictionary, entry,
                                                value, attributes);
  }

  static void AddImpl(Handle<JSObject> object, uint32_t entry,
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
    FastElementsAccessorSubclass::SetImpl(object->elements(), entry, *value);
  }

  static void DeleteImpl(Handle<JSObject> obj, uint32_t entry) {
    ElementsKind kind = KindTraits::Kind;
    if (IsFastPackedElementsKind(kind)) {
      JSObject::TransitionElementsKind(obj, GetHoleyElementsKind(kind));
    }
    if (IsFastSmiOrObjectElementsKind(KindTraits::Kind)) {
      JSObject::EnsureWritableFastElements(obj);
    }
    DeleteCommon(obj, entry, handle(obj->elements()));
  }

  static bool HasEntryImpl(FixedArrayBase* backing_store, uint32_t entry) {
    return !BackingStore::cast(backing_store)->is_the_hole(entry);
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
      case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
      case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
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
      case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
      case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
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

  typedef typename ElementsKindTraits<Kind>::BackingStore BackingStore;
  typedef TypedElementsAccessor<Kind> AccessorClass;

  static Handle<Object> GetImpl(Handle<JSObject> obj, uint32_t index,
                                Handle<FixedArrayBase> backing_store) {
    if (index < AccessorClass::GetCapacityImpl(*obj, *backing_store)) {
      return BackingStore::get(Handle<BackingStore>::cast(backing_store),
                               index);
    } else {
      return backing_store->GetIsolate()->factory()->undefined_value();
    }
  }

  static PropertyDetails GetDetailsImpl(FixedArrayBase* backing_store,
                                        uint32_t entry) {
    return PropertyDetails(DONT_DELETE, DATA, 0, PropertyCellType::kNoCell);
  }

  static void SetLengthImpl(Handle<JSArray> array, uint32_t length,
                            Handle<FixedArrayBase> backing_store) {
    // External arrays do not support changing their length.
    UNREACHABLE();
  }

  static void DeleteImpl(Handle<JSObject> obj, uint32_t entry) {
    UNREACHABLE();
  }

  static uint32_t GetEntryForIndexImpl(JSObject* holder,
                                       FixedArrayBase* backing_store,
                                       uint32_t index) {
    return index < AccessorClass::GetCapacityImpl(holder, backing_store)
               ? index
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


template <typename SloppyArgumentsElementsAccessorSubclass,
          typename ArgumentsAccessor, typename KindTraits>
class SloppyArgumentsElementsAccessor
    : public ElementsAccessorBase<SloppyArgumentsElementsAccessorSubclass,
                                  KindTraits> {
 public:
  explicit SloppyArgumentsElementsAccessor(const char* name)
      : ElementsAccessorBase<SloppyArgumentsElementsAccessorSubclass,
                             KindTraits>(name) {}

  static Handle<Object> GetImpl(Handle<JSObject> obj, uint32_t index,
                                Handle<FixedArrayBase> parameters) {
    Isolate* isolate = obj->GetIsolate();
    Handle<FixedArray> parameter_map = Handle<FixedArray>::cast(parameters);
    Handle<Object> probe(GetParameterMapArg(*parameter_map, index), isolate);
    if (!probe->IsTheHole()) {
      DisallowHeapAllocation no_gc;
      Context* context = Context::cast(parameter_map->get(0));
      int context_entry = Handle<Smi>::cast(probe)->value();
      DCHECK(!context->get(context_entry)->IsTheHole());
      return handle(context->get(context_entry), isolate);
    } else {
      // Object is not mapped, defer to the arguments.
      Handle<FixedArray> arguments(FixedArray::cast(parameter_map->get(1)),
                                   isolate);
      Handle<Object> result = ArgumentsAccessor::GetImpl(obj, index, arguments);
      // Elements of the arguments object in slow mode might be slow aliases.
      if (result->IsAliasedArgumentsEntry()) {
        DisallowHeapAllocation no_gc;
        AliasedArgumentsEntry* entry = AliasedArgumentsEntry::cast(*result);
        Context* context = Context::cast(parameter_map->get(0));
        int context_entry = entry->aliased_context_slot();
        DCHECK(!context->get(context_entry)->IsTheHole());
        return handle(context->get(context_entry), isolate);
      } else {
        return result;
      }
    }
  }

  static void GrowCapacityAndConvertImpl(Handle<JSObject> object,
                                         uint32_t capacity) {
    UNREACHABLE();
  }

  static void SetImpl(FixedArrayBase* store, uint32_t index, Object* value) {
    FixedArray* parameter_map = FixedArray::cast(store);
    Object* probe = GetParameterMapArg(parameter_map, index);
    if (!probe->IsTheHole()) {
      Context* context = Context::cast(parameter_map->get(0));
      int context_entry = Smi::cast(probe)->value();
      DCHECK(!context->get(context_entry)->IsTheHole());
      context->set(context_entry, value);
    } else {
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      ArgumentsAccessor::SetImpl(arguments, index, value);
    }
  }

  static void SetLengthImpl(Handle<JSArray> array, uint32_t length,
                            Handle<FixedArrayBase> parameter_map) {
    // Sloppy arguments objects are not arrays.
    UNREACHABLE();
  }

  static uint32_t GetCapacityImpl(JSObject* holder,
                                  FixedArrayBase* backing_store) {
    FixedArray* parameter_map = FixedArray::cast(backing_store);
    FixedArrayBase* arguments = FixedArrayBase::cast(parameter_map->get(1));
    return parameter_map->length() - 2 +
           ArgumentsAccessor::GetCapacityImpl(holder, arguments);
  }

  static bool HasEntryImpl(FixedArrayBase* parameters, uint32_t entry) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    uint32_t length = parameter_map->length() - 2;
    if (entry < length) {
      return !GetParameterMapArg(parameter_map, entry)->IsTheHole();
    }

    FixedArrayBase* arguments = FixedArrayBase::cast(parameter_map->get(1));
    return ArgumentsAccessor::HasEntryImpl(arguments, entry - length);
  }

  static uint32_t GetIndexForEntryImpl(FixedArrayBase* parameters,
                                       uint32_t entry) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    uint32_t length = parameter_map->length() - 2;
    if (entry < length) return entry;

    FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
    return ArgumentsAccessor::GetIndexForEntryImpl(arguments, entry - length);
  }

  static uint32_t GetEntryForIndexImpl(JSObject* holder,
                                       FixedArrayBase* parameters,
                                       uint32_t index) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    Object* probe = GetParameterMapArg(parameter_map, index);
    if (!probe->IsTheHole()) return index;

    FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
    uint32_t entry =
        ArgumentsAccessor::GetEntryForIndexImpl(holder, arguments, index);
    if (entry == kMaxUInt32) return entry;
    return (parameter_map->length() - 2) + entry;
  }

  static PropertyDetails GetDetailsImpl(FixedArrayBase* parameters,
                                        uint32_t entry) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    uint32_t length = parameter_map->length() - 2;
    if (entry < length) {
      return PropertyDetails(NONE, DATA, 0, PropertyCellType::kNoCell);
    }
    entry -= length;
    FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
    return ArgumentsAccessor::GetDetailsImpl(arguments, entry);
  }

  static Object* GetParameterMapArg(FixedArray* parameter_map, uint32_t index) {
    uint32_t length = parameter_map->length() - 2;
    return index < length
               ? parameter_map->get(index + 2)
               : Object::cast(parameter_map->GetHeap()->the_hole_value());
  }

  static void DeleteImpl(Handle<JSObject> obj, uint32_t entry) {
    FixedArray* parameter_map = FixedArray::cast(obj->elements());
    uint32_t length = static_cast<uint32_t>(parameter_map->length()) - 2;
    if (entry < length) {
      // TODO(kmillikin): We could check if this was the last aliased
      // parameter, and revert to normal elements in that case.  That
      // would enable GC of the context.
      parameter_map->set_the_hole(entry + 2);
    } else {
      SloppyArgumentsElementsAccessorSubclass::DeleteFromArguments(
          obj, entry - length);
    }
  }
};


class SlowSloppyArgumentsElementsAccessor
    : public SloppyArgumentsElementsAccessor<
          SlowSloppyArgumentsElementsAccessor, DictionaryElementsAccessor,
          ElementsKindTraits<SLOW_SLOPPY_ARGUMENTS_ELEMENTS> > {
 public:
  explicit SlowSloppyArgumentsElementsAccessor(const char* name)
      : SloppyArgumentsElementsAccessor<
            SlowSloppyArgumentsElementsAccessor, DictionaryElementsAccessor,
            ElementsKindTraits<SLOW_SLOPPY_ARGUMENTS_ELEMENTS> >(name) {}

  static void DeleteFromArguments(Handle<JSObject> obj, uint32_t entry) {
    Handle<FixedArray> parameter_map(FixedArray::cast(obj->elements()));
    Handle<SeededNumberDictionary> dict(
        SeededNumberDictionary::cast(parameter_map->get(1)));
    // TODO(verwaest): Remove reliance on index in Shrink.
    uint32_t index = GetIndexForEntryImpl(*dict, entry);
    Handle<Object> result = SeededNumberDictionary::DeleteProperty(dict, entry);
    USE(result);
    DCHECK(result->IsTrue());
    Handle<FixedArray> new_elements =
        SeededNumberDictionary::Shrink(dict, index);
    parameter_map->set(1, *new_elements);
  }

  static void AddImpl(Handle<JSObject> object, uint32_t index,
                      Handle<Object> value, PropertyAttributes attributes,
                      uint32_t new_capacity) {
    Handle<FixedArray> parameter_map(FixedArray::cast(object->elements()));
    Handle<FixedArrayBase> old_elements(
        FixedArrayBase::cast(parameter_map->get(1)));
    Handle<SeededNumberDictionary> dictionary =
        old_elements->IsSeededNumberDictionary()
            ? Handle<SeededNumberDictionary>::cast(old_elements)
            : JSObject::NormalizeElements(object);
    PropertyDetails details(attributes, DATA, 0, PropertyCellType::kNoCell);
    Handle<SeededNumberDictionary> new_dictionary =
        SeededNumberDictionary::AddNumberEntry(dictionary, index, value,
                                               details);
    if (attributes != NONE) new_dictionary->set_requires_slow_elements();
    if (*dictionary != *new_dictionary) {
      FixedArray::cast(object->elements())->set(1, *new_dictionary);
    }
  }

  static void ReconfigureImpl(Handle<JSObject> object,
                              Handle<FixedArrayBase> store, uint32_t entry,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
    Handle<FixedArray> parameter_map = Handle<FixedArray>::cast(store);
    uint32_t length = parameter_map->length() - 2;
    if (entry < length) {
      Object* probe = parameter_map->get(entry + 2);
      DCHECK(!probe->IsTheHole());
      Context* context = Context::cast(parameter_map->get(0));
      int context_entry = Smi::cast(probe)->value();
      DCHECK(!context->get(context_entry)->IsTheHole());
      context->set(context_entry, *value);

      // Redefining attributes of an aliased element destroys fast aliasing.
      parameter_map->set_the_hole(entry + 2);
      // For elements that are still writable we re-establish slow aliasing.
      if ((attributes & READ_ONLY) == 0) {
        Isolate* isolate = store->GetIsolate();
        value = isolate->factory()->NewAliasedArgumentsEntry(context_entry);
      }

      PropertyDetails details(attributes, DATA, 0, PropertyCellType::kNoCell);
      Handle<SeededNumberDictionary> arguments(
          SeededNumberDictionary::cast(parameter_map->get(1)));
      arguments = SeededNumberDictionary::AddNumberEntry(arguments, entry,
                                                         value, details);
      parameter_map->set(1, *arguments);
    } else {
      Handle<FixedArrayBase> arguments(
          FixedArrayBase::cast(parameter_map->get(1)));
      DictionaryElementsAccessor::ReconfigureImpl(
          object, arguments, entry - length, value, attributes);
    }
  }
};


class FastSloppyArgumentsElementsAccessor
    : public SloppyArgumentsElementsAccessor<
          FastSloppyArgumentsElementsAccessor, FastHoleyObjectElementsAccessor,
          ElementsKindTraits<FAST_SLOPPY_ARGUMENTS_ELEMENTS> > {
 public:
  explicit FastSloppyArgumentsElementsAccessor(const char* name)
      : SloppyArgumentsElementsAccessor<
            FastSloppyArgumentsElementsAccessor,
            FastHoleyObjectElementsAccessor,
            ElementsKindTraits<FAST_SLOPPY_ARGUMENTS_ELEMENTS> >(name) {}

  static void DeleteFromArguments(Handle<JSObject> obj, uint32_t entry) {
    FixedArray* parameter_map = FixedArray::cast(obj->elements());
    Handle<FixedArray> arguments(FixedArray::cast(parameter_map->get(1)));
    FastHoleyObjectElementsAccessor::DeleteCommon(obj, entry, arguments);
  }

  static void AddImpl(Handle<JSObject> object, uint32_t index,
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
    SetImpl(object->elements(), index, *value);
  }

  static void ReconfigureImpl(Handle<JSObject> object,
                              Handle<FixedArrayBase> store, uint32_t entry,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
    Handle<SeededNumberDictionary> dictionary =
        JSObject::NormalizeElements(object);
    FixedArray::cast(*store)->set(1, *dictionary);
    uint32_t length = static_cast<uint32_t>(store->length()) - 2;
    if (entry >= length) {
      entry = dictionary->FindEntry(entry - length) + length;
    }
    SlowSloppyArgumentsElementsAccessor::ReconfigureImpl(object, store, entry,
                                                         value, attributes);
  }

  static void CopyElementsImpl(FixedArrayBase* from, uint32_t from_start,
                               FixedArrayBase* to, ElementsKind from_kind,
                               uint32_t to_start, int packed_size,
                               int copy_size) {
    DCHECK(!to->IsDictionary());
    if (from_kind == SLOW_SLOPPY_ARGUMENTS_ELEMENTS) {
      CopyDictionaryToObjectElements(from, from_start, to, FAST_HOLEY_ELEMENTS,
                                     to_start, copy_size);
    } else {
      DCHECK_EQ(FAST_SLOPPY_ARGUMENTS_ELEMENTS, from_kind);
      CopyObjectToObjectElements(from, FAST_HOLEY_ELEMENTS, from_start, to,
                                 FAST_HOLEY_ELEMENTS, to_start, copy_size);
    }
  }

  static void GrowCapacityAndConvertImpl(Handle<JSObject> object,
                                         uint32_t capacity) {
    Handle<FixedArray> parameter_map(FixedArray::cast(object->elements()));
    Handle<FixedArray> old_elements(FixedArray::cast(parameter_map->get(1)));
    ElementsKind from_kind = object->GetElementsKind();
    // This method should only be called if there's a reason to update the
    // elements.
    DCHECK(from_kind == SLOW_SLOPPY_ARGUMENTS_ELEMENTS ||
           static_cast<uint32_t>(old_elements->length()) < capacity);
    Handle<FixedArrayBase> elements =
        ConvertElementsWithCapacity(object, old_elements, from_kind, capacity);
    Handle<Map> new_map = JSObject::GetElementsTransitionMap(
        object, FAST_SLOPPY_ARGUMENTS_ELEMENTS);
    JSObject::MigrateToMap(object, new_map);
    parameter_map->set(1, *elements);
    JSObject::ValidateElements(object);
  }
};


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
}  // namespace


void CheckArrayAbuse(Handle<JSObject> obj, const char* op, uint32_t index,
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
      if (index >= compare_length) {
        PrintF("[OOB %s %s (%s length = %d, element accessed = %d) in ",
               elements_type, op, elements_type, static_cast<int>(int32_length),
               static_cast<int>(index));
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
      for (int entry = 0; entry < number_of_elements; entry++) {
        smi_elms->set(entry, (*args)[entry], SKIP_WRITE_BARRIER);
      }
      break;
    }
    case FAST_HOLEY_ELEMENTS:
    case FAST_ELEMENTS: {
      DisallowHeapAllocation no_gc;
      WriteBarrierMode mode = elms->GetWriteBarrierMode(no_gc);
      Handle<FixedArray> object_elms = Handle<FixedArray>::cast(elms);
      for (int entry = 0; entry < number_of_elements; entry++) {
        object_elms->set(entry, (*args)[entry], mode);
      }
      break;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      Handle<FixedDoubleArray> double_elms =
          Handle<FixedDoubleArray>::cast(elms);
      for (int entry = 0; entry < number_of_elements; entry++) {
        double_elms->set(entry, (*args)[entry]->Number());
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


ElementsAccessor** ElementsAccessor::elements_accessors_ = NULL;
}  // namespace internal
}  // namespace v8

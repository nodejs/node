// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/elements.h"

#include "src/arguments.h"
#include "src/conversions.h"
#include "src/factory.h"
#include "src/messages.h"
#include "src/objects-inl.h"
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
//   - StringWrapperElementsAccessor
//     - FastStringWrapperElementsAccessor
//     - SlowStringWrapperElementsAccessor

namespace v8 {
namespace internal {


namespace {


static const int kPackedSizeNotKnown = -1;

enum Where { AT_START, AT_END };


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
  V(FastStringWrapperElementsAccessor, FAST_STRING_WRAPPER_ELEMENTS,          \
    FixedArray)                                                               \
  V(SlowStringWrapperElementsAccessor, SLOW_STRING_WRAPPER_ELEMENTS,          \
    FixedArray)                                                               \
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


MUST_USE_RESULT
MaybeHandle<Object> ThrowArrayLengthRangeError(Isolate* isolate) {
  THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kInvalidArrayLength),
                  Object);
}


void CopyObjectToObjectElements(FixedArrayBase* from_base,
                                ElementsKind from_kind, uint32_t from_start,
                                FixedArrayBase* to_base, ElementsKind to_kind,
                                uint32_t to_start, int raw_copy_size) {
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
  DCHECK(IsFastSmiOrObjectElementsKind(from_kind) ||
         from_kind == FAST_STRING_WRAPPER_ELEMENTS);
  DCHECK(IsFastSmiOrObjectElementsKind(to_kind));

  WriteBarrierMode write_barrier_mode =
      ((IsFastObjectElementsKind(from_kind) &&
        IsFastObjectElementsKind(to_kind)) ||
       from_kind == FAST_STRING_WRAPPER_ELEMENTS)
          ? UPDATE_WRITE_BARRIER
          : SKIP_WRITE_BARRIER;
  for (int i = 0; i < copy_size; i++) {
    Object* value = from->get(from_start + i);
    to->set(to_start + i, value, write_barrier_mode);
  }
}


static void CopyDictionaryToObjectElements(
    FixedArrayBase* from_base, uint32_t from_start, FixedArrayBase* to_base,
    ElementsKind to_kind, uint32_t to_start, int raw_copy_size) {
  DisallowHeapAllocation no_allocation;
  SeededNumberDictionary* from = SeededNumberDictionary::cast(from_base);
  int copy_size = raw_copy_size;
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
  WriteBarrierMode write_barrier_mode = IsFastObjectElementsKind(to_kind)
                                            ? UPDATE_WRITE_BARRIER
                                            : SKIP_WRITE_BARRIER;
  for (int i = 0; i < copy_size; i++) {
    int entry = from->FindEntry(i + from_start);
    if (entry != SeededNumberDictionary::kNotFound) {
      Object* value = from->ValueAt(entry);
      DCHECK(!value->IsTheHole());
      to->set(i + to_start, value, write_barrier_mode);
    } else {
      to->set_the_hole(i + to_start);
    }
  }
}


// NOTE: this method violates the handlified function signature convention:
// raw pointer parameters in the function that allocates.
// See ElementsAccessorBase::CopyElements() for details.
static void CopyDoubleToObjectElements(FixedArrayBase* from_base,
                                       uint32_t from_start,
                                       FixedArrayBase* to_base,
                                       uint32_t to_start, int raw_copy_size) {
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

  // Use an outer loop to not waste too much time on creating HandleScopes.
  // On the other hand we might overflow a single handle scope depending on
  // the copy_size.
  int offset = 0;
  while (offset < copy_size) {
    HandleScope scope(isolate);
    offset += 100;
    for (int i = offset - 100; i < offset && i < copy_size; ++i) {
      Handle<Object> value =
          FixedDoubleArray::get(*from, i + from_start, isolate);
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
    Code* apply_builtin =
        isolate->builtins()->builtin(Builtins::kFunctionPrototypeApply);
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

  bool IsPacked(Handle<JSObject> holder, Handle<FixedArrayBase> backing_store,
                uint32_t start, uint32_t end) final {
    return ElementsAccessorSubclass::IsPackedImpl(holder, backing_store, start,
                                                  end);
  }

  static bool IsPackedImpl(Handle<JSObject> holder,
                           Handle<FixedArrayBase> backing_store, uint32_t start,
                           uint32_t end) {
    if (IsFastPackedElementsKind(kind())) return true;
    for (uint32_t i = start; i < end; i++) {
      if (!ElementsAccessorSubclass::HasElementImpl(holder, i, backing_store,
                                                    ALL_PROPERTIES)) {
        return false;
      }
    }
    return true;
  }

  static void TryTransitionResultArrayToPacked(Handle<JSArray> array) {
    if (!IsHoleyElementsKind(kind())) return;
    int length = Smi::cast(array->length())->value();
    Handle<FixedArrayBase> backing_store(array->elements());
    if (!ElementsAccessorSubclass::IsPackedImpl(array, backing_store, 0,
                                                length)) {
      return;
    }
    ElementsKind packed_kind = GetPackedElementsKind(kind());
    Handle<Map> new_map =
        JSObject::GetElementsTransitionMap(array, packed_kind);
    JSObject::MigrateToMap(array, new_map);
    if (FLAG_trace_elements_transitions) {
      JSObject::PrintElementsTransition(stdout, array, kind(), backing_store,
                                        packed_kind, backing_store);
    }
  }

  bool HasElement(Handle<JSObject> holder, uint32_t index,
                  Handle<FixedArrayBase> backing_store,
                  PropertyFilter filter) final {
    return ElementsAccessorSubclass::HasElementImpl(holder, index,
                                                    backing_store, filter);
  }

  static bool HasElementImpl(Handle<JSObject> holder, uint32_t index,
                             Handle<FixedArrayBase> backing_store,
                             PropertyFilter filter) {
    return ElementsAccessorSubclass::GetEntryForIndexImpl(
               *holder, *backing_store, index, filter) != kMaxUInt32;
  }

  bool HasAccessors(JSObject* holder) final {
    return ElementsAccessorSubclass::HasAccessorsImpl(holder,
                                                      holder->elements());
  }

  static bool HasAccessorsImpl(JSObject* holder,
                               FixedArrayBase* backing_store) {
    return false;
  }

  Handle<Object> Get(Handle<JSObject> holder, uint32_t entry) final {
    return ElementsAccessorSubclass::GetImpl(holder, entry);
  }

  static Handle<Object> GetImpl(Handle<JSObject> holder, uint32_t entry) {
    return ElementsAccessorSubclass::GetImpl(holder->elements(), entry);
  }

  static Handle<Object> GetImpl(FixedArrayBase* backing_store, uint32_t entry) {
    Isolate* isolate = backing_store->GetIsolate();
    uint32_t index = GetIndexForEntryImpl(backing_store, entry);
    return handle(BackingStore::cast(backing_store)->get(index), isolate);
  }

  void Set(Handle<JSObject> holder, uint32_t entry, Object* value) final {
    ElementsAccessorSubclass::SetImpl(holder, entry, value);
  }

  void Reconfigure(Handle<JSObject> object, Handle<FixedArrayBase> store,
                   uint32_t entry, Handle<Object> value,
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

  void Add(Handle<JSObject> object, uint32_t index, Handle<Object> value,
           PropertyAttributes attributes, uint32_t new_capacity) final {
    ElementsAccessorSubclass::AddImpl(object, index, value, attributes,
                                      new_capacity);
  }

  static void AddImpl(Handle<JSObject> object, uint32_t index,
                      Handle<Object> value, PropertyAttributes attributes,
                      uint32_t new_capacity) {
    UNREACHABLE();
  }

  uint32_t Push(Handle<JSArray> receiver, Handle<FixedArrayBase> backing_store,
                Arguments* args, uint32_t push_size) final {
    return ElementsAccessorSubclass::PushImpl(receiver, backing_store, args,
                                              push_size);
  }

  static uint32_t PushImpl(Handle<JSArray> receiver,
                           Handle<FixedArrayBase> elms_obj, Arguments* args,
                           uint32_t push_sized) {
    UNREACHABLE();
    return 0;
  }

  uint32_t Unshift(Handle<JSArray> receiver,
                   Handle<FixedArrayBase> backing_store, Arguments* args,
                   uint32_t unshift_size) final {
    return ElementsAccessorSubclass::UnshiftImpl(receiver, backing_store, args,
                                                 unshift_size);
  }

  static uint32_t UnshiftImpl(Handle<JSArray> receiver,
                              Handle<FixedArrayBase> elms_obj, Arguments* args,
                              uint32_t unshift_size) {
    UNREACHABLE();
    return 0;
  }

  Handle<JSArray> Slice(Handle<JSObject> receiver,
                        Handle<FixedArrayBase> backing_store, uint32_t start,
                        uint32_t end) final {
    return ElementsAccessorSubclass::SliceImpl(receiver, backing_store, start,
                                               end);
  }

  static Handle<JSArray> SliceImpl(Handle<JSObject> receiver,
                                   Handle<FixedArrayBase> backing_store,
                                   uint32_t start, uint32_t end) {
    UNREACHABLE();
    return Handle<JSArray>();
  }

  Handle<JSArray> Splice(Handle<JSArray> receiver,
                         Handle<FixedArrayBase> backing_store, uint32_t start,
                         uint32_t delete_count, Arguments* args,
                         uint32_t add_count) final {
    return ElementsAccessorSubclass::SpliceImpl(receiver, backing_store, start,
                                                delete_count, args, add_count);
  }

  static Handle<JSArray> SpliceImpl(Handle<JSArray> receiver,
                                    Handle<FixedArrayBase> backing_store,
                                    uint32_t start, uint32_t delete_count,
                                    Arguments* args, uint32_t add_count) {
    UNREACHABLE();
    return Handle<JSArray>();
  }

  Handle<Object> Pop(Handle<JSArray> receiver,
                     Handle<FixedArrayBase> backing_store) final {
    return ElementsAccessorSubclass::PopImpl(receiver, backing_store);
  }

  static Handle<Object> PopImpl(Handle<JSArray> receiver,
                                Handle<FixedArrayBase> backing_store) {
    UNREACHABLE();
    return Handle<Object>();
  }

  Handle<Object> Shift(Handle<JSArray> receiver,
                       Handle<FixedArrayBase> backing_store) final {
    return ElementsAccessorSubclass::ShiftImpl(receiver, backing_store);
  }

  static Handle<Object> ShiftImpl(Handle<JSArray> receiver,
                                  Handle<FixedArrayBase> backing_store) {
    UNREACHABLE();
    return Handle<Object>();
  }

  void SetLength(Handle<JSArray> array, uint32_t length) final {
    ElementsAccessorSubclass::SetLengthImpl(array->GetIsolate(), array, length,
                                            handle(array->elements()));
  }

  static void SetLengthImpl(Isolate* isolate, Handle<JSArray> array,
                            uint32_t length,
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
    old_length = Min(old_length, capacity);
    if (length == 0) {
      array->initialize_elements();
    } else if (length <= capacity) {
      if (array->HasFastSmiOrObjectElements()) {
        backing_store = JSObject::EnsureWritableFastElements(array);
      }
      if (2 * length <= capacity) {
        // If more than half the elements won't be used, trim the array.
        isolate->heap()->RightTrimFixedArray<Heap::CONCURRENT_TO_SWEEPER>(
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

  static Handle<FixedArrayBase> ConvertElementsWithCapacity(
      Handle<JSObject> object, Handle<FixedArrayBase> old_elements,
      ElementsKind from_kind, uint32_t capacity) {
    return ConvertElementsWithCapacity(
        object, old_elements, from_kind, capacity, 0, 0,
        ElementsAccessor::kCopyToEndAndInitializeToHole);
  }

  static Handle<FixedArrayBase> ConvertElementsWithCapacity(
      Handle<JSObject> object, Handle<FixedArrayBase> old_elements,
      ElementsKind from_kind, uint32_t capacity, int copy_size) {
    return ConvertElementsWithCapacity(object, old_elements, from_kind,
                                       capacity, 0, 0, copy_size);
  }

  static Handle<FixedArrayBase> ConvertElementsWithCapacity(
      Handle<JSObject> object, Handle<FixedArrayBase> old_elements,
      ElementsKind from_kind, uint32_t capacity, uint32_t src_index,
      uint32_t dst_index, int copy_size) {
    Isolate* isolate = object->GetIsolate();
    Handle<FixedArrayBase> new_elements;
    if (IsFastDoubleElementsKind(kind())) {
      new_elements = isolate->factory()->NewFixedDoubleArray(capacity);
    } else {
      new_elements = isolate->factory()->NewUninitializedFixedArray(capacity);
    }

    int packed_size = kPackedSizeNotKnown;
    if (IsFastPackedElementsKind(from_kind) && object->IsJSArray()) {
      packed_size = Smi::cast(JSArray::cast(*object)->length())->value();
    }

    ElementsAccessorSubclass::CopyElementsImpl(
        *old_elements, src_index, *new_elements, from_kind, dst_index,
        packed_size, copy_size);

    return new_elements;
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
           from_kind == SLOW_STRING_WRAPPER_ELEMENTS ||
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

  void GrowCapacityAndConvert(Handle<JSObject> object,
                              uint32_t capacity) final {
    ElementsAccessorSubclass::GrowCapacityAndConvertImpl(object, capacity);
  }

  void Delete(Handle<JSObject> obj, uint32_t entry) final {
    ElementsAccessorSubclass::DeleteImpl(obj, entry);
  }

  static void CopyElementsImpl(FixedArrayBase* from, uint32_t from_start,
                               FixedArrayBase* to, ElementsKind from_kind,
                               uint32_t to_start, int packed_size,
                               int copy_size) {
    UNREACHABLE();
  }

  void CopyElements(JSObject* from_holder, uint32_t from_start,
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

  static void CollectElementIndicesImpl(Handle<JSObject> object,
                                        Handle<FixedArrayBase> backing_store,
                                        KeyAccumulator* keys, uint32_t range,
                                        PropertyFilter filter,
                                        uint32_t offset) {
    DCHECK_NE(DICTIONARY_ELEMENTS, kind());
    if (filter & ONLY_ALL_CAN_READ) {
      // Non-dictionary elements can't have all-can-read accessors.
      return;
    }
    uint32_t length = 0;
    if (object->IsJSArray()) {
      length = Smi::cast(JSArray::cast(*object)->length())->value();
    } else {
      length =
          ElementsAccessorSubclass::GetCapacityImpl(*object, *backing_store);
    }
    if (range < length) length = range;
    for (uint32_t i = offset; i < length; i++) {
      if (!ElementsAccessorSubclass::HasElementImpl(object, i, backing_store,
                                                    filter)) {
        continue;
      }
      keys->AddKey(i);
    }
  }

  void CollectElementIndices(Handle<JSObject> object,
                             Handle<FixedArrayBase> backing_store,
                             KeyAccumulator* keys, uint32_t range,
                             PropertyFilter filter, uint32_t offset) final {
    ElementsAccessorSubclass::CollectElementIndicesImpl(
        object, backing_store, keys, range, filter, offset);
  };

  void AddElementsToKeyAccumulator(Handle<JSObject> receiver,
                                   KeyAccumulator* accumulator,
                                   AddKeyConversion convert) final {
    ElementsAccessorSubclass::AddElementsToKeyAccumulatorImpl(
        receiver, accumulator, convert);
  }

  static uint32_t GetCapacityImpl(JSObject* holder,
                                  FixedArrayBase* backing_store) {
    return backing_store->length();
  }

  uint32_t GetCapacity(JSObject* holder, FixedArrayBase* backing_store) final {
    return ElementsAccessorSubclass::GetCapacityImpl(holder, backing_store);
  }

  static uint32_t GetIndexForEntryImpl(FixedArrayBase* backing_store,
                                       uint32_t entry) {
    return entry;
  }

  static uint32_t GetEntryForIndexImpl(JSObject* holder,
                                       FixedArrayBase* backing_store,
                                       uint32_t index, PropertyFilter filter) {
    if (IsHoleyElementsKind(kind())) {
      return index < ElementsAccessorSubclass::GetCapacityImpl(holder,
                                                               backing_store) &&
                     !BackingStore::cast(backing_store)->is_the_hole(index)
                 ? index
                 : kMaxUInt32;
    } else {
      uint32_t length =
          holder->IsJSArray()
              ? static_cast<uint32_t>(
                    Smi::cast(JSArray::cast(holder)->length())->value())
              : ElementsAccessorSubclass::GetCapacityImpl(holder,
                                                          backing_store);
      return index < length ? index : kMaxUInt32;
    }
  }

  uint32_t GetEntryForIndex(JSObject* holder, FixedArrayBase* backing_store,
                            uint32_t index) final {
    return ElementsAccessorSubclass::GetEntryForIndexImpl(
        holder, backing_store, index, ALL_PROPERTIES);
  }

  static PropertyDetails GetDetailsImpl(FixedArrayBase* backing_store,
                                        uint32_t entry) {
    return PropertyDetails(NONE, DATA, 0, PropertyCellType::kNoCell);
  }

  static PropertyDetails GetDetailsImpl(JSObject* holder, uint32_t entry) {
    return PropertyDetails(NONE, DATA, 0, PropertyCellType::kNoCell);
  }

  PropertyDetails GetDetails(JSObject* holder, uint32_t entry) final {
    return ElementsAccessorSubclass::GetDetailsImpl(holder, entry);
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

  static void SetLengthImpl(Isolate* isolate, Handle<JSArray> array,
                            uint32_t length,
                            Handle<FixedArrayBase> backing_store) {
    Handle<SeededNumberDictionary> dict =
        Handle<SeededNumberDictionary>::cast(backing_store);
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

  static bool HasAccessorsImpl(JSObject* holder,
                               FixedArrayBase* backing_store) {
    SeededNumberDictionary* dict = SeededNumberDictionary::cast(backing_store);
    if (!dict->requires_slow_elements()) return false;
    int capacity = dict->Capacity();
    for (int i = 0; i < capacity; i++) {
      Object* key = dict->KeyAt(i);
      if (!dict->IsKey(key)) continue;
      DCHECK(!dict->IsDeleted(i));
      PropertyDetails details = dict->DetailsAt(i);
      if (details.type() == ACCESSOR_CONSTANT) return true;
    }
    return false;
  }

  static Object* GetRaw(FixedArrayBase* store, uint32_t entry) {
    SeededNumberDictionary* backing_store = SeededNumberDictionary::cast(store);
    return backing_store->ValueAt(entry);
  }

  static Handle<Object> GetImpl(Handle<JSObject> holder, uint32_t entry) {
    return GetImpl(holder->elements(), entry);
  }

  static Handle<Object> GetImpl(FixedArrayBase* backing_store, uint32_t entry) {
    return handle(GetRaw(backing_store, entry), backing_store->GetIsolate());
  }

  static inline void SetImpl(Handle<JSObject> holder, uint32_t entry,
                             Object* value) {
    SetImpl(holder->elements(), entry, value);
  }

  static inline void SetImpl(FixedArrayBase* backing_store, uint32_t entry,
                             Object* value) {
    SeededNumberDictionary::cast(backing_store)->ValueAtPut(entry, value);
  }

  static void ReconfigureImpl(Handle<JSObject> object,
                              Handle<FixedArrayBase> store, uint32_t entry,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
    SeededNumberDictionary* dictionary = SeededNumberDictionary::cast(*store);
    if (attributes != NONE) object->RequireSlowElements(dictionary);
    dictionary->ValueAtPut(entry, *value);
    PropertyDetails details = dictionary->DetailsAt(entry);
    details = PropertyDetails(attributes, DATA, details.dictionary_index(),
                              PropertyCellType::kNoCell);
    dictionary->DetailsAtPut(entry, details);
  }

  static void AddImpl(Handle<JSObject> object, uint32_t index,
                      Handle<Object> value, PropertyAttributes attributes,
                      uint32_t new_capacity) {
    PropertyDetails details(attributes, DATA, 0, PropertyCellType::kNoCell);
    Handle<SeededNumberDictionary> dictionary =
        object->HasFastElements() || object->HasFastStringWrapperElements()
            ? JSObject::NormalizeElements(object)
            : handle(SeededNumberDictionary::cast(object->elements()));
    Handle<SeededNumberDictionary> new_dictionary =
        SeededNumberDictionary::AddNumberEntry(
            dictionary, index, value, details,
            object->map()->is_prototype_map());
    if (attributes != NONE) object->RequireSlowElements(*new_dictionary);
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
                                       uint32_t index, PropertyFilter filter) {
    DisallowHeapAllocation no_gc;
    SeededNumberDictionary* dictionary = SeededNumberDictionary::cast(store);
    int entry = dictionary->FindEntry(index);
    if (entry == SeededNumberDictionary::kNotFound) return kMaxUInt32;
    if (filter != ALL_PROPERTIES) {
      PropertyDetails details = dictionary->DetailsAt(entry);
      PropertyAttributes attr = details.attributes();
      if ((attr & filter) != 0) return kMaxUInt32;
    }
    return static_cast<uint32_t>(entry);
  }

  static PropertyDetails GetDetailsImpl(JSObject* holder, uint32_t entry) {
    return GetDetailsImpl(holder->elements(), entry);
  }

  static PropertyDetails GetDetailsImpl(FixedArrayBase* backing_store,
                                        uint32_t entry) {
    return SeededNumberDictionary::cast(backing_store)->DetailsAt(entry);
  }

  static void CollectElementIndicesImpl(Handle<JSObject> object,
                                        Handle<FixedArrayBase> backing_store,
                                        KeyAccumulator* keys, uint32_t range,
                                        PropertyFilter filter,
                                        uint32_t offset) {
    Handle<SeededNumberDictionary> dictionary =
        Handle<SeededNumberDictionary>::cast(backing_store);
    int capacity = dictionary->Capacity();
    for (int i = 0; i < capacity; i++) {
      Object* k = dictionary->KeyAt(i);
      if (!dictionary->IsKey(k)) continue;
      if (k->FilterKey(filter)) continue;
      if (dictionary->IsDeleted(i)) continue;
      DCHECK(k->IsNumber());
      DCHECK_LE(k->Number(), kMaxUInt32);
      uint32_t index = static_cast<uint32_t>(k->Number());
      if (index < offset) continue;
      PropertyDetails details = dictionary->DetailsAt(i);
      if (filter & ONLY_ALL_CAN_READ) {
        if (details.kind() != kAccessor) continue;
        Object* accessors = dictionary->ValueAt(i);
        if (!accessors->IsAccessorInfo()) continue;
        if (!AccessorInfo::cast(accessors)->all_can_read()) continue;
      }
      PropertyAttributes attr = details.attributes();
      if ((attr & filter) != 0) continue;
      keys->AddKey(index);
    }

    keys->SortCurrentElementsList();
  }

  static void AddElementsToKeyAccumulatorImpl(Handle<JSObject> receiver,
                                              KeyAccumulator* accumulator,
                                              AddKeyConversion convert) {
    SeededNumberDictionary* dictionary =
        SeededNumberDictionary::cast(receiver->elements());
    int capacity = dictionary->Capacity();
    for (int i = 0; i < capacity; i++) {
      Object* k = dictionary->KeyAt(i);
      if (!dictionary->IsKey(k)) continue;
      if (dictionary->IsDeleted(i)) continue;
      Object* value = dictionary->ValueAt(i);
      DCHECK(!value->IsTheHole());
      DCHECK(!value->IsAccessorPair());
      DCHECK(!value->IsAccessorInfo());
      accumulator->AddKey(value, convert);
    }
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
    DCHECK(obj->HasFastSmiOrObjectElements() || obj->HasFastDoubleElements() ||
           obj->HasFastArgumentsElements() ||
           obj->HasFastStringWrapperElements());
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
        if (!backing_store->is_the_hole(i)) {
          ++num_used;
          // Bail out if a number dictionary wouldn't be able to save at least
          // 75% space.
          if (4 * SeededNumberDictionary::ComputeCapacity(num_used) *
                  SeededNumberDictionary::kEntrySize >
              backing_store->length()) {
            return;
          }
        }
      }
      JSObject::NormalizeElements(obj);
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
      if (IsFastElementsKind(from_kind) && from_kind != to_kind) {
        JSObject::TransitionElementsKind(object, to_kind);
      }
      if (IsFastSmiOrObjectElementsKind(from_kind)) {
        DCHECK(IsFastSmiOrObjectElementsKind(to_kind));
        JSObject::EnsureWritableFastElements(object);
      }
    }
    FastElementsAccessorSubclass::SetImpl(object, index, *value);
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

  static void AddElementsToKeyAccumulatorImpl(Handle<JSObject> receiver,
                                              KeyAccumulator* accumulator,
                                              AddKeyConversion convert) {
    uint32_t length = 0;
    Handle<FixedArrayBase> elements(receiver->elements(),
                                    receiver->GetIsolate());
    if (receiver->IsJSArray()) {
      length = Smi::cast(JSArray::cast(*receiver)->length())->value();
    } else {
      length =
          FastElementsAccessorSubclass::GetCapacityImpl(*receiver, *elements);
    }
    for (uint32_t i = 0; i < length; i++) {
      if (IsFastPackedElementsKind(KindTraits::Kind) ||
          HasEntryImpl(*elements, i)) {
        accumulator->AddKey(FastElementsAccessorSubclass::GetImpl(*elements, i),
                            convert);
      }
    }
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
        DCHECK(BackingStore::get(*backing_store, i, isolate)->IsSmi() ||
               (IsFastHoleyElementsKind(KindTraits::Kind) &&
                backing_store->is_the_hole(i)));
      }
    }
#endif
  }

  static Handle<Object> PopImpl(Handle<JSArray> receiver,
                                Handle<FixedArrayBase> backing_store) {
    return FastElementsAccessorSubclass::RemoveElement(receiver, backing_store,
                                                       AT_END);
  }

  static Handle<Object> ShiftImpl(Handle<JSArray> receiver,
                                  Handle<FixedArrayBase> backing_store) {
    return FastElementsAccessorSubclass::RemoveElement(receiver, backing_store,
                                                       AT_START);
  }

  static uint32_t PushImpl(Handle<JSArray> receiver,
                           Handle<FixedArrayBase> backing_store,
                           Arguments* args, uint32_t push_size) {
    return FastElementsAccessorSubclass::AddArguments(receiver, backing_store,
                                                      args, push_size, AT_END);
  }

  static uint32_t UnshiftImpl(Handle<JSArray> receiver,
                              Handle<FixedArrayBase> backing_store,
                              Arguments* args, uint32_t unshift_size) {
    return FastElementsAccessorSubclass::AddArguments(
        receiver, backing_store, args, unshift_size, AT_START);
  }

  static void MoveElements(Isolate* isolate, Handle<JSArray> receiver,
                           Handle<FixedArrayBase> backing_store, int dst_index,
                           int src_index, int len, int hole_start,
                           int hole_end) {
    UNREACHABLE();
  }

  static Handle<JSArray> SliceImpl(Handle<JSObject> receiver,
                                   Handle<FixedArrayBase> backing_store,
                                   uint32_t start, uint32_t end) {
    DCHECK(start < end);
    Isolate* isolate = receiver->GetIsolate();
    int result_len = end - start;
    Handle<JSArray> result_array = isolate->factory()->NewJSArray(
        KindTraits::Kind, result_len, result_len);
    DisallowHeapAllocation no_gc;
    FastElementsAccessorSubclass::CopyElementsImpl(
        *backing_store, start, result_array->elements(), KindTraits::Kind, 0,
        kPackedSizeNotKnown, result_len);
    FastElementsAccessorSubclass::TryTransitionResultArrayToPacked(
        result_array);
    return result_array;
  }

  static Handle<JSArray> SpliceImpl(Handle<JSArray> receiver,
                                    Handle<FixedArrayBase> backing_store,
                                    uint32_t start, uint32_t delete_count,
                                    Arguments* args, uint32_t add_count) {
    Isolate* isolate = receiver->GetIsolate();
    Heap* heap = isolate->heap();
    uint32_t length = Smi::cast(receiver->length())->value();
    uint32_t new_length = length - delete_count + add_count;

    if (new_length == 0) {
      receiver->set_elements(heap->empty_fixed_array());
      receiver->set_length(Smi::FromInt(0));
      return isolate->factory()->NewJSArrayWithElements(
          backing_store, KindTraits::Kind, delete_count);
    }

    // Construct the result array which holds the deleted elements.
    Handle<JSArray> deleted_elements = isolate->factory()->NewJSArray(
        KindTraits::Kind, delete_count, delete_count);
    if (delete_count > 0) {
      DisallowHeapAllocation no_gc;
      FastElementsAccessorSubclass::CopyElementsImpl(
          *backing_store, start, deleted_elements->elements(), KindTraits::Kind,
          0, kPackedSizeNotKnown, delete_count);
    }

    // Delete and move elements to make space for add_count new elements.
    if (add_count < delete_count) {
      FastElementsAccessorSubclass::SpliceShrinkStep(
          isolate, receiver, backing_store, start, delete_count, add_count,
          length, new_length);
    } else if (add_count > delete_count) {
      backing_store = FastElementsAccessorSubclass::SpliceGrowStep(
          isolate, receiver, backing_store, start, delete_count, add_count,
          length, new_length);
    }

    // Copy over the arguments.
    FastElementsAccessorSubclass::CopyArguments(args, backing_store, add_count,
                                                3, start);

    receiver->set_length(Smi::FromInt(new_length));
    FastElementsAccessorSubclass::TryTransitionResultArrayToPacked(
        deleted_elements);
    return deleted_elements;
  }

 private:
  // SpliceShrinkStep might modify the backing_store.
  static void SpliceShrinkStep(Isolate* isolate, Handle<JSArray> receiver,
                               Handle<FixedArrayBase> backing_store,
                               uint32_t start, uint32_t delete_count,
                               uint32_t add_count, uint32_t len,
                               uint32_t new_length) {
    const int move_left_count = len - delete_count - start;
    const int move_left_dst_index = start + add_count;
    FastElementsAccessorSubclass::MoveElements(
        isolate, receiver, backing_store, move_left_dst_index,
        start + delete_count, move_left_count, new_length, len);
  }

  // SpliceGrowStep might modify the backing_store.
  static Handle<FixedArrayBase> SpliceGrowStep(
      Isolate* isolate, Handle<JSArray> receiver,
      Handle<FixedArrayBase> backing_store, uint32_t start,
      uint32_t delete_count, uint32_t add_count, uint32_t length,
      uint32_t new_length) {
    // Check we do not overflow the new_length.
    DCHECK((add_count - delete_count) <= (Smi::kMaxValue - length));
    // Check if backing_store is big enough.
    if (new_length <= static_cast<uint32_t>(backing_store->length())) {
      FastElementsAccessorSubclass::MoveElements(
          isolate, receiver, backing_store, start + add_count,
          start + delete_count, (length - delete_count - start), 0, 0);
      // MoveElements updates the backing_store in-place.
      return backing_store;
    }
    // New backing storage is needed.
    int capacity = JSObject::NewElementsCapacity(new_length);
    // Partially copy all elements up to start.
    Handle<FixedArrayBase> new_elms =
        FastElementsAccessorSubclass::ConvertElementsWithCapacity(
            receiver, backing_store, KindTraits::Kind, capacity, start);
    // Copy the trailing elements after start + delete_count
    FastElementsAccessorSubclass::CopyElementsImpl(
        *backing_store, start + delete_count, *new_elms, KindTraits::Kind,
        start + add_count, kPackedSizeNotKnown,
        ElementsAccessor::kCopyToEndAndInitializeToHole);
    receiver->set_elements(*new_elms);
    return new_elms;
  }

  static Handle<Object> RemoveElement(Handle<JSArray> receiver,
                                      Handle<FixedArrayBase> backing_store,
                                      Where remove_position) {
    Isolate* isolate = receiver->GetIsolate();
    uint32_t length =
        static_cast<uint32_t>(Smi::cast(receiver->length())->value());
    DCHECK(length > 0);
    int new_length = length - 1;
    int remove_index = remove_position == AT_START ? 0 : new_length;
    Handle<Object> result =
        FastElementsAccessorSubclass::GetImpl(*backing_store, remove_index);
    if (remove_position == AT_START) {
      FastElementsAccessorSubclass::MoveElements(
          isolate, receiver, backing_store, 0, 1, new_length, 0, 0);
    }
    FastElementsAccessorSubclass::SetLengthImpl(isolate, receiver, new_length,
                                                backing_store);

    if (IsHoleyElementsKind(KindTraits::Kind) && result->IsTheHole()) {
      return receiver->GetIsolate()->factory()->undefined_value();
    }
    return result;
  }

  static uint32_t AddArguments(Handle<JSArray> receiver,
                               Handle<FixedArrayBase> backing_store,
                               Arguments* args, uint32_t add_size,
                               Where remove_position) {
    uint32_t length = Smi::cast(receiver->length())->value();
    DCHECK(add_size > 0);
    uint32_t elms_len = backing_store->length();
    // Check we do not overflow the new_length.
    DCHECK(add_size <= static_cast<uint32_t>(Smi::kMaxValue - length));
    uint32_t new_length = length + add_size;

    if (new_length > elms_len) {
      // New backing storage is needed.
      uint32_t capacity = JSObject::NewElementsCapacity(new_length);
      // If we add arguments to the start we have to shift the existing objects.
      int copy_dst_index = remove_position == AT_START ? add_size : 0;
      // Copy over all objects to a new backing_store.
      backing_store = FastElementsAccessorSubclass::ConvertElementsWithCapacity(
          receiver, backing_store, KindTraits::Kind, capacity, 0,
          copy_dst_index, ElementsAccessor::kCopyToEndAndInitializeToHole);
      receiver->set_elements(*backing_store);
    } else if (remove_position == AT_START) {
      // If the backing store has enough capacity and we add elements to the
      // start we have to shift the existing objects.
      Isolate* isolate = receiver->GetIsolate();
      FastElementsAccessorSubclass::MoveElements(
          isolate, receiver, backing_store, add_size, 0, length, 0, 0);
    }

    int insertion_index = remove_position == AT_START ? 0 : length;
    // Copy the arguments to the start.
    FastElementsAccessorSubclass::CopyArguments(args, backing_store, add_size,
                                                1, insertion_index);
    // Set the length.
    receiver->set_length(Smi::FromInt(new_length));
    return new_length;
  }

  static void CopyArguments(Arguments* args, Handle<FixedArrayBase> dst_store,
                            uint32_t copy_size, uint32_t src_index,
                            uint32_t dst_index) {
    // Add the provided values.
    DisallowHeapAllocation no_gc;
    FixedArrayBase* raw_backing_store = *dst_store;
    WriteBarrierMode mode = raw_backing_store->GetWriteBarrierMode(no_gc);
    for (uint32_t i = 0; i < copy_size; i++) {
      Object* argument = (*args)[i + src_index];
      FastElementsAccessorSubclass::SetImpl(raw_backing_store, i + dst_index,
                                            argument, mode);
    }
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

  static inline void SetImpl(Handle<JSObject> holder, uint32_t entry,
                             Object* value) {
    SetImpl(holder->elements(), entry, value);
  }

  static inline void SetImpl(FixedArrayBase* backing_store, uint32_t entry,
                             Object* value) {
    FixedArray::cast(backing_store)->set(entry, value);
  }

  static inline void SetImpl(FixedArrayBase* backing_store, uint32_t entry,
                             Object* value, WriteBarrierMode mode) {
    FixedArray::cast(backing_store)->set(entry, value, mode);
  }

  static Object* GetRaw(FixedArray* backing_store, uint32_t entry) {
    uint32_t index = FastElementsAccessorSubclass::GetIndexForEntryImpl(
        backing_store, entry);
    return backing_store->get(index);
  }

  static void MoveElements(Isolate* isolate, Handle<JSArray> receiver,
                           Handle<FixedArrayBase> backing_store, int dst_index,
                           int src_index, int len, int hole_start,
                           int hole_end) {
    Heap* heap = isolate->heap();
    Handle<FixedArray> dst_elms = Handle<FixedArray>::cast(backing_store);
    if (heap->CanMoveObjectStart(*dst_elms) && dst_index == 0) {
      // Update all the copies of this backing_store handle.
      *dst_elms.location() =
          FixedArray::cast(heap->LeftTrimFixedArray(*dst_elms, src_index));
      receiver->set_elements(*dst_elms);
      // Adjust the hole offset as the array has been shrunk.
      hole_end -= src_index;
      DCHECK_LE(hole_start, backing_store->length());
      DCHECK_LE(hole_end, backing_store->length());
    } else if (len != 0) {
      DisallowHeapAllocation no_gc;
      heap->MoveElements(*dst_elms, dst_index, src_index, len);
    }
    if (hole_start != hole_end) {
      dst_elms->FillWithHoles(hole_start, hole_end);
    }
  }

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
      case FAST_STRING_WRAPPER_ELEMENTS:
        CopyObjectToObjectElements(from, from_kind, from_start, to, to_kind,
                                   to_start, copy_size);
        break;
      case FAST_DOUBLE_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS: {
        AllowHeapAllocation allow_allocation;
        DCHECK(IsFastObjectElementsKind(to_kind));
        CopyDoubleToObjectElements(from, from_start, to, to_start, copy_size);
        break;
      }
      case DICTIONARY_ELEMENTS:
      case SLOW_STRING_WRAPPER_ELEMENTS:
        CopyDictionaryToObjectElements(from, from_start, to, to_kind, to_start,
                                       copy_size);
        break;
      case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
      case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) case TYPE##_ELEMENTS:
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      // This function is currently only used for JSArrays with non-zero
      // length.
      UNREACHABLE();
      break;
      case NO_ELEMENTS:
        break;  // Nothing to do.
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

  static Handle<Object> GetImpl(Handle<JSObject> holder, uint32_t entry) {
    return GetImpl(holder->elements(), entry);
  }

  static Handle<Object> GetImpl(FixedArrayBase* backing_store, uint32_t entry) {
    Isolate* isolate = backing_store->GetIsolate();
    return FixedDoubleArray::get(FixedDoubleArray::cast(backing_store), entry,
                                 isolate);
  }

  static inline void SetImpl(Handle<JSObject> holder, uint32_t entry,
                             Object* value) {
    SetImpl(holder->elements(), entry, value);
  }

  static inline void SetImpl(FixedArrayBase* backing_store, uint32_t entry,
                             Object* value) {
    FixedDoubleArray::cast(backing_store)->set(entry, value->Number());
  }

  static inline void SetImpl(FixedArrayBase* backing_store, uint32_t entry,
                             Object* value, WriteBarrierMode mode) {
    FixedDoubleArray::cast(backing_store)->set(entry, value->Number());
  }

  static void MoveElements(Isolate* isolate, Handle<JSArray> receiver,
                           Handle<FixedArrayBase> backing_store, int dst_index,
                           int src_index, int len, int hole_start,
                           int hole_end) {
    Heap* heap = isolate->heap();
    Handle<FixedDoubleArray> dst_elms =
        Handle<FixedDoubleArray>::cast(backing_store);
    if (heap->CanMoveObjectStart(*dst_elms) && dst_index == 0) {
      // Update all the copies of this backing_store handle.
      *dst_elms.location() = FixedDoubleArray::cast(
          heap->LeftTrimFixedArray(*dst_elms, src_index));
      receiver->set_elements(*dst_elms);
      // Adjust the hole offset as the array has been shrunk.
      hole_end -= src_index;
      DCHECK_LE(hole_start, backing_store->length());
      DCHECK_LE(hole_end, backing_store->length());
    } else if (len != 0) {
      MemMove(dst_elms->data_start() + dst_index,
              dst_elms->data_start() + src_index, len * kDoubleSize);
    }
    if (hole_start != hole_end) {
      dst_elms->FillWithHoles(hole_start, hole_end);
    }
  }

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
      case FAST_STRING_WRAPPER_ELEMENTS:
      case SLOW_STRING_WRAPPER_ELEMENTS:
      case NO_ELEMENTS:
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) case TYPE##_ELEMENTS:
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      // This function is currently only used for JSArrays with non-zero
      // length.
      UNREACHABLE();
      break;
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

  static inline void SetImpl(Handle<JSObject> holder, uint32_t entry,
                             Object* value) {
    SetImpl(holder->elements(), entry, value);
  }

  static inline void SetImpl(FixedArrayBase* backing_store, uint32_t entry,
                             Object* value) {
    BackingStore::cast(backing_store)->SetValue(entry, value);
  }

  static inline void SetImpl(FixedArrayBase* backing_store, uint32_t entry,
                             Object* value, WriteBarrierMode mode) {
    BackingStore::cast(backing_store)->SetValue(entry, value);
  }

  static Handle<Object> GetImpl(Handle<JSObject> holder, uint32_t entry) {
    return GetImpl(holder->elements(), entry);
  }

  static Handle<Object> GetImpl(FixedArrayBase* backing_store, uint32_t entry) {
    return BackingStore::get(BackingStore::cast(backing_store), entry);
  }

  static PropertyDetails GetDetailsImpl(JSObject* holder, uint32_t entry) {
    return PropertyDetails(DONT_DELETE, DATA, 0, PropertyCellType::kNoCell);
  }

  static PropertyDetails GetDetailsImpl(FixedArrayBase* backing_store,
                                        uint32_t entry) {
    return PropertyDetails(DONT_DELETE, DATA, 0, PropertyCellType::kNoCell);
  }

  static bool HasElementImpl(Handle<JSObject> holder, uint32_t index,
                             Handle<FixedArrayBase> backing_store,
                             PropertyFilter filter) {
    return index < AccessorClass::GetCapacityImpl(*holder, *backing_store);
  }

  static bool HasAccessorsImpl(JSObject* holder,
                               FixedArrayBase* backing_store) {
    return false;
  }

  static void SetLengthImpl(Isolate* isolate, Handle<JSArray> array,
                            uint32_t length,
                            Handle<FixedArrayBase> backing_store) {
    // External arrays do not support changing their length.
    UNREACHABLE();
  }

  static void DeleteImpl(Handle<JSObject> obj, uint32_t entry) {
    UNREACHABLE();
  }

  static uint32_t GetIndexForEntryImpl(FixedArrayBase* backing_store,
                                       uint32_t entry) {
    return entry;
  }

  static uint32_t GetEntryForIndexImpl(JSObject* holder,
                                       FixedArrayBase* backing_store,
                                       uint32_t index, PropertyFilter filter) {
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

  static void AddElementsToKeyAccumulatorImpl(Handle<JSObject> receiver,
                                              KeyAccumulator* accumulator,
                                              AddKeyConversion convert) {
    Handle<FixedArrayBase> elements(receiver->elements(),
                                    receiver->GetIsolate());
    uint32_t length = AccessorClass::GetCapacityImpl(*receiver, *elements);
    for (uint32_t i = 0; i < length; i++) {
      Handle<Object> value = AccessorClass::GetImpl(*elements, i);
      accumulator->AddKey(value, convert);
    }
  }
};



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
                             KindTraits>(name) {
    USE(KindTraits::Kind);
  }

  static Handle<Object> GetImpl(Handle<JSObject> holder, uint32_t entry) {
    return GetImpl(holder->elements(), entry);
  }

  static Handle<Object> GetImpl(FixedArrayBase* parameters, uint32_t entry) {
    Isolate* isolate = parameters->GetIsolate();
    Handle<FixedArray> parameter_map(FixedArray::cast(parameters), isolate);
    uint32_t length = parameter_map->length() - 2;
    if (entry < length) {
      DisallowHeapAllocation no_gc;
      Object* probe = parameter_map->get(entry + 2);
      Context* context = Context::cast(parameter_map->get(0));
      int context_entry = Smi::cast(probe)->value();
      DCHECK(!context->get(context_entry)->IsTheHole());
      return handle(context->get(context_entry), isolate);
    } else {
      // Object is not mapped, defer to the arguments.
      Handle<Object> result = ArgumentsAccessor::GetImpl(
          FixedArray::cast(parameter_map->get(1)), entry - length);
      // Elements of the arguments object in slow mode might be slow aliases.
      if (result->IsAliasedArgumentsEntry()) {
        DisallowHeapAllocation no_gc;
        AliasedArgumentsEntry* alias = AliasedArgumentsEntry::cast(*result);
        Context* context = Context::cast(parameter_map->get(0));
        int context_entry = alias->aliased_context_slot();
        DCHECK(!context->get(context_entry)->IsTheHole());
        return handle(context->get(context_entry), isolate);
      }
      return result;
    }
  }

  static void GrowCapacityAndConvertImpl(Handle<JSObject> object,
                                         uint32_t capacity) {
    UNREACHABLE();
  }

  static inline void SetImpl(Handle<JSObject> holder, uint32_t entry,
                             Object* value) {
    SetImpl(holder->elements(), entry, value);
  }

  static inline void SetImpl(FixedArrayBase* store, uint32_t entry,
                             Object* value) {
    FixedArray* parameter_map = FixedArray::cast(store);
    uint32_t length = parameter_map->length() - 2;
    if (entry < length) {
      Object* probe = parameter_map->get(entry + 2);
      Context* context = Context::cast(parameter_map->get(0));
      int context_entry = Smi::cast(probe)->value();
      DCHECK(!context->get(context_entry)->IsTheHole());
      context->set(context_entry, value);
    } else {
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      Object* current = ArgumentsAccessor::GetRaw(arguments, entry - length);
      if (current->IsAliasedArgumentsEntry()) {
        AliasedArgumentsEntry* alias = AliasedArgumentsEntry::cast(current);
        Context* context = Context::cast(parameter_map->get(0));
        int context_entry = alias->aliased_context_slot();
        DCHECK(!context->get(context_entry)->IsTheHole());
        context->set(context_entry, value);
      } else {
        ArgumentsAccessor::SetImpl(arguments, entry - length, value);
      }
    }
  }

  static void SetLengthImpl(Isolate* isolate, Handle<JSArray> array,
                            uint32_t length,
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

  static void AddElementsToKeyAccumulatorImpl(Handle<JSObject> receiver,
                                              KeyAccumulator* accumulator,
                                              AddKeyConversion convert) {
    FixedArrayBase* elements = receiver->elements();
    uint32_t length = GetCapacityImpl(*receiver, elements);
    for (uint32_t entry = 0; entry < length; entry++) {
      if (!HasEntryImpl(elements, entry)) continue;
      Handle<Object> value = GetImpl(elements, entry);
      accumulator->AddKey(value, convert);
    }
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

  static bool HasAccessorsImpl(JSObject* holder,
                               FixedArrayBase* backing_store) {
    FixedArray* parameter_map = FixedArray::cast(backing_store);
    FixedArrayBase* arguments = FixedArrayBase::cast(parameter_map->get(1));
    return ArgumentsAccessor::HasAccessorsImpl(holder, arguments);
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
                                       uint32_t index, PropertyFilter filter) {
    FixedArray* parameter_map = FixedArray::cast(parameters);
    Object* probe = GetParameterMapArg(parameter_map, index);
    if (!probe->IsTheHole()) return index;

    FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
    uint32_t entry = ArgumentsAccessor::GetEntryForIndexImpl(holder, arguments,
                                                             index, filter);
    if (entry == kMaxUInt32) return entry;
    return (parameter_map->length() - 2) + entry;
  }

  static PropertyDetails GetDetailsImpl(JSObject* holder, uint32_t entry) {
    FixedArray* parameter_map = FixedArray::cast(holder->elements());
    uint32_t length = parameter_map->length() - 2;
    if (entry < length) {
      return PropertyDetails(NONE, DATA, 0, PropertyCellType::kNoCell);
    }
    FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
    return ArgumentsAccessor::GetDetailsImpl(arguments, entry - length);
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
        SeededNumberDictionary::AddNumberEntry(
            dictionary, index, value, details,
            object->map()->is_prototype_map());
    if (attributes != NONE) object->RequireSlowElements(*new_dictionary);
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
      arguments = SeededNumberDictionary::AddNumberEntry(
          arguments, entry, value, details, object->map()->is_prototype_map());
      // If the attributes were NONE, we would have called set rather than
      // reconfigure.
      DCHECK_NE(NONE, attributes);
      object->RequireSlowElements(*arguments);
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
    FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
    // For fast holey objects, the entry equals the index. The code above made
    // sure that there's enough space to store the value. We cannot convert
    // index to entry explicitly since the slot still contains the hole, so the
    // current EntryForIndex would indicate that it is "absent" by returning
    // kMaxUInt32.
    FastHoleyObjectElementsAccessor::SetImpl(arguments, index, *value);
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

template <typename StringWrapperElementsAccessorSubclass,
          typename BackingStoreAccessor, typename KindTraits>
class StringWrapperElementsAccessor
    : public ElementsAccessorBase<StringWrapperElementsAccessorSubclass,
                                  KindTraits> {
 public:
  explicit StringWrapperElementsAccessor(const char* name)
      : ElementsAccessorBase<StringWrapperElementsAccessorSubclass, KindTraits>(
            name) {
    USE(KindTraits::Kind);
  }

  static Handle<Object> GetImpl(Handle<JSObject> holder, uint32_t entry) {
    Isolate* isolate = holder->GetIsolate();
    Handle<String> string(GetString(*holder), isolate);
    uint32_t length = static_cast<uint32_t>(string->length());
    if (entry < length) {
      return isolate->factory()->LookupSingleCharacterStringFromCode(
          String::Flatten(string)->Get(entry));
    }
    return BackingStoreAccessor::GetImpl(holder, entry - length);
  }

  static PropertyDetails GetDetailsImpl(JSObject* holder, uint32_t entry) {
    uint32_t length = static_cast<uint32_t>(GetString(holder)->length());
    if (entry < length) {
      PropertyAttributes attributes =
          static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE);
      return PropertyDetails(attributes, v8::internal::DATA, 0,
                             PropertyCellType::kNoCell);
    }
    return BackingStoreAccessor::GetDetailsImpl(holder, entry - length);
  }

  static uint32_t GetEntryForIndexImpl(JSObject* holder,
                                       FixedArrayBase* backing_store,
                                       uint32_t index, PropertyFilter filter) {
    uint32_t length = static_cast<uint32_t>(GetString(holder)->length());
    if (index < length) return index;
    uint32_t backing_store_entry = BackingStoreAccessor::GetEntryForIndexImpl(
        holder, backing_store, index, filter);
    if (backing_store_entry == kMaxUInt32) return kMaxUInt32;
    DCHECK(backing_store_entry < kMaxUInt32 - length);
    return backing_store_entry + length;
  }

  static void DeleteImpl(Handle<JSObject> holder, uint32_t entry) {
    uint32_t length = static_cast<uint32_t>(GetString(*holder)->length());
    if (entry < length) {
      return;  // String contents can't be deleted.
    }
    BackingStoreAccessor::DeleteImpl(holder, entry - length);
  }

  static void SetImpl(Handle<JSObject> holder, uint32_t entry, Object* value) {
    uint32_t length = static_cast<uint32_t>(GetString(*holder)->length());
    if (entry < length) {
      return;  // String contents are read-only.
    }
    BackingStoreAccessor::SetImpl(holder->elements(), entry - length, value);
  }

  static void AddImpl(Handle<JSObject> object, uint32_t index,
                      Handle<Object> value, PropertyAttributes attributes,
                      uint32_t new_capacity) {
    DCHECK(index >= static_cast<uint32_t>(GetString(*object)->length()));
    // Explicitly grow fast backing stores if needed. Dictionaries know how to
    // extend their capacity themselves.
    if (KindTraits::Kind == FAST_STRING_WRAPPER_ELEMENTS &&
        (object->GetElementsKind() == SLOW_STRING_WRAPPER_ELEMENTS ||
         BackingStoreAccessor::GetCapacityImpl(*object, object->elements()) !=
             new_capacity)) {
      StringWrapperElementsAccessorSubclass::GrowCapacityAndConvertImpl(
          object, new_capacity);
    }
    BackingStoreAccessor::AddImpl(object, index, value, attributes,
                                  new_capacity);
  }

  static void ReconfigureImpl(Handle<JSObject> object,
                              Handle<FixedArrayBase> store, uint32_t entry,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
    uint32_t length = static_cast<uint32_t>(GetString(*object)->length());
    if (entry < length) {
      return;  // String contents can't be reconfigured.
    }
    BackingStoreAccessor::ReconfigureImpl(object, store, entry - length, value,
                                          attributes);
  }

  static void AddElementsToKeyAccumulatorImpl(Handle<JSObject> receiver,
                                              KeyAccumulator* accumulator,
                                              AddKeyConversion convert) {
    Isolate* isolate = receiver->GetIsolate();
    Handle<String> string(GetString(*receiver), isolate);
    string = String::Flatten(string);
    uint32_t length = static_cast<uint32_t>(string->length());
    for (uint32_t i = 0; i < length; i++) {
      accumulator->AddKey(
          isolate->factory()->LookupSingleCharacterStringFromCode(
              string->Get(i)),
          convert);
    }
    BackingStoreAccessor::AddElementsToKeyAccumulatorImpl(receiver, accumulator,
                                                          convert);
  }

  static void CollectElementIndicesImpl(Handle<JSObject> object,
                                        Handle<FixedArrayBase> backing_store,
                                        KeyAccumulator* keys, uint32_t range,
                                        PropertyFilter filter,
                                        uint32_t offset) {
    if ((filter & ONLY_ALL_CAN_READ) == 0) {
      uint32_t length = GetString(*object)->length();
      for (uint32_t i = 0; i < length; i++) {
        keys->AddKey(i);
      }
    }
    BackingStoreAccessor::CollectElementIndicesImpl(object, backing_store, keys,
                                                    range, filter, offset);
  }

  static void CopyElementsImpl(FixedArrayBase* from, uint32_t from_start,
                               FixedArrayBase* to, ElementsKind from_kind,
                               uint32_t to_start, int packed_size,
                               int copy_size) {
    BackingStoreAccessor::CopyElementsImpl(from, from_start, to, from_kind,
                                           to_start, packed_size, copy_size);
  }

 private:
  static String* GetString(JSObject* holder) {
    DCHECK(holder->IsJSValue());
    JSValue* js_value = JSValue::cast(holder);
    DCHECK(js_value->value()->IsString());
    return String::cast(js_value->value());
  }
};

class FastStringWrapperElementsAccessor
    : public StringWrapperElementsAccessor<
          FastStringWrapperElementsAccessor, FastHoleyObjectElementsAccessor,
          ElementsKindTraits<FAST_STRING_WRAPPER_ELEMENTS>> {
 public:
  explicit FastStringWrapperElementsAccessor(const char* name)
      : StringWrapperElementsAccessor<
            FastStringWrapperElementsAccessor, FastHoleyObjectElementsAccessor,
            ElementsKindTraits<FAST_STRING_WRAPPER_ELEMENTS>>(name) {}
};

class SlowStringWrapperElementsAccessor
    : public StringWrapperElementsAccessor<
          SlowStringWrapperElementsAccessor, DictionaryElementsAccessor,
          ElementsKindTraits<SLOW_STRING_WRAPPER_ELEMENTS>> {
 public:
  explicit SlowStringWrapperElementsAccessor(const char* name)
      : StringWrapperElementsAccessor<
            SlowStringWrapperElementsAccessor, DictionaryElementsAccessor,
            ElementsKindTraits<SLOW_STRING_WRAPPER_ELEMENTS>>(name) {}

  static bool HasAccessorsImpl(JSObject* holder,
                               FixedArrayBase* backing_store) {
    return DictionaryElementsAccessor::HasAccessorsImpl(holder, backing_store);
  }
};

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
    if (length > 0 && length < JSArray::kInitialMaxFastElementArray) {
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


Handle<JSArray> ElementsAccessor::Concat(Isolate* isolate, Arguments* args,
                                         uint32_t concat_size) {
  int result_len = 0;
  ElementsKind elements_kind = GetInitialFastElementsKind();
  bool has_double = false;
  {
    DisallowHeapAllocation no_gc;
    // Iterate through all the arguments performing checks
    // and calculating total length.
    bool is_holey = false;
    for (uint32_t i = 0; i < concat_size; i++) {
      Object* arg = (*args)[i];
      int len = Smi::cast(JSArray::cast(arg)->length())->value();

      // We shouldn't overflow when adding another len.
      const int kHalfOfMaxInt = 1 << (kBitsPerInt - 2);
      STATIC_ASSERT(FixedArray::kMaxLength < kHalfOfMaxInt);
      USE(kHalfOfMaxInt);
      result_len += len;
      DCHECK(0 <= result_len);
      DCHECK(result_len <= FixedDoubleArray::kMaxLength);

      ElementsKind arg_kind = JSArray::cast(arg)->map()->elements_kind();
      has_double = has_double || IsFastDoubleElementsKind(arg_kind);
      is_holey = is_holey || IsFastHoleyElementsKind(arg_kind);
      elements_kind = GetMoreGeneralElementsKind(elements_kind, arg_kind);
    }
    if (is_holey) {
      elements_kind = GetHoleyElementsKind(elements_kind);
    }
  }

  // If a double array is concatted into a fast elements array, the fast
  // elements array needs to be initialized to contain proper holes, since
  // boxing doubles may cause incremental marking.
  ArrayStorageAllocationMode mode =
      has_double && IsFastObjectElementsKind(elements_kind)
          ? INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE
          : DONT_INITIALIZE_ARRAY_ELEMENTS;
  Handle<JSArray> result_array = isolate->factory()->NewJSArray(
      elements_kind, result_len, result_len, Strength::WEAK, mode);
  if (result_len == 0) return result_array;
  int j = 0;
  Handle<FixedArrayBase> storage(result_array->elements(), isolate);
  ElementsAccessor* accessor = ElementsAccessor::ForKind(elements_kind);
  for (uint32_t i = 0; i < concat_size; i++) {
    // It is crucial to keep |array| in a raw pointer form to avoid
    // performance degradation.
    JSArray* array = JSArray::cast((*args)[i]);
    int len = Smi::cast(array->length())->value();
    if (len > 0) {
      ElementsKind from_kind = array->GetElementsKind();
      accessor->CopyElements(array, 0, from_kind, storage, j, len);
      j += len;
    }
  }

  DCHECK(j == result_len);
  return result_array;
}

ElementsAccessor** ElementsAccessor::elements_accessors_ = NULL;
}  // namespace internal
}  // namespace v8

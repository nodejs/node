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
//   - ExternalElementsAccessor                  (abstract)
//     - ExternalByteElementsAccessor
//     - ExternalUnsignedByteElementsAccessor
//     - ExternalShortElementsAccessor
//     - ExternalUnsignedShortElementsAccessor
//     - ExternalIntElementsAccessor
//     - ExternalUnsignedIntElementsAccessor
//     - ExternalFloatElementsAccessor
//     - ExternalDoubleElementsAccessor
//     - PixelElementsAccessor
//   - DictionaryElementsAccessor
//   - NonStrictArgumentsElementsAccessor


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
  V(NonStrictArgumentsElementsAccessor, NON_STRICT_ARGUMENTS_ELEMENTS,  \
    FixedArray)                                                         \
  V(ExternalByteElementsAccessor, EXTERNAL_BYTE_ELEMENTS,               \
    ExternalByteArray)                                                  \
  V(ExternalUnsignedByteElementsAccessor,                               \
    EXTERNAL_UNSIGNED_BYTE_ELEMENTS, ExternalUnsignedByteArray)         \
  V(ExternalShortElementsAccessor, EXTERNAL_SHORT_ELEMENTS,             \
    ExternalShortArray)                                                 \
  V(ExternalUnsignedShortElementsAccessor,                              \
    EXTERNAL_UNSIGNED_SHORT_ELEMENTS, ExternalUnsignedShortArray)       \
  V(ExternalIntElementsAccessor, EXTERNAL_INT_ELEMENTS,                 \
    ExternalIntArray)                                                   \
  V(ExternalUnsignedIntElementsAccessor,                                \
    EXTERNAL_UNSIGNED_INT_ELEMENTS, ExternalUnsignedIntArray)           \
  V(ExternalFloatElementsAccessor,                                      \
    EXTERNAL_FLOAT_ELEMENTS, ExternalFloatArray)                        \
  V(ExternalDoubleElementsAccessor,                                     \
    EXTERNAL_DOUBLE_ELEMENTS, ExternalDoubleArray)                      \
  V(PixelElementsAccessor, EXTERNAL_PIXEL_ELEMENTS, ExternalPixelArray)


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


static Failure* ThrowArrayLengthRangeError(Heap* heap) {
  HandleScope scope(heap->isolate());
  return heap->isolate()->Throw(
      *heap->isolate()->factory()->NewRangeError("invalid_array_length",
          HandleVector<Object>(NULL, 0)));
}


static void CopyObjectToObjectElements(FixedArrayBase* from_base,
                                       ElementsKind from_kind,
                                       uint32_t from_start,
                                       FixedArrayBase* to_base,
                                       ElementsKind to_kind,
                                       uint32_t to_start,
                                       int raw_copy_size) {
  ASSERT(to_base->map() != HEAP->fixed_cow_array_map());
  AssertNoAllocation no_allocation;
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
        MemsetPointer(FixedArray::cast(to_base)->data_start() + start,
                      heap->the_hole_value(), length);
      }
    }
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  FixedArray* from = FixedArray::cast(from_base);
  FixedArray* to = FixedArray::cast(to_base);
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
    if (!heap->InNewSpace(to)) {
      heap->RecordWrites(to->address(),
                         to->OffsetOfElementAt(to_start),
                         copy_size);
    }
    heap->incremental_marking()->RecordWrites(to);
  }
}


static void CopyDictionaryToObjectElements(FixedArrayBase* from_base,
                                           uint32_t from_start,
                                           FixedArrayBase* to_base,
                                           ElementsKind to_kind,
                                           uint32_t to_start,
                                           int raw_copy_size) {
  SeededNumberDictionary* from = SeededNumberDictionary::cast(from_base);
  AssertNoAllocation no_allocation;
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
        MemsetPointer(FixedArray::cast(to_base)->data_start() + start,
                      heap->the_hole_value(), length);
      }
    }
  }
  ASSERT(to_base != from_base);
  ASSERT(IsFastSmiOrObjectElementsKind(to_kind));
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
      ASSERT(!value->IsTheHole());
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


MUST_USE_RESULT static MaybeObject* CopyDoubleToObjectElements(
    FixedArrayBase* from_base,
    uint32_t from_start,
    FixedArrayBase* to_base,
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
        MemsetPointer(FixedArray::cast(to_base)->data_start() + start,
                      heap->the_hole_value(), length);
      }
    }
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return from_base;
  FixedDoubleArray* from = FixedDoubleArray::cast(from_base);
  FixedArray* to = FixedArray::cast(to_base);
  for (int i = 0; i < copy_size; ++i) {
    if (IsFastSmiElementsKind(to_kind)) {
      UNIMPLEMENTED();
      return Failure::Exception();
    } else {
      MaybeObject* maybe_value = from->get(i + from_start);
      Object* value;
      ASSERT(IsFastObjectElementsKind(to_kind));
      // Because Double -> Object elements transitions allocate HeapObjects
      // iteratively, the allocate must succeed within a single GC cycle,
      // otherwise the retry after the GC will also fail. In order to ensure
      // that no GC is triggered, allocate HeapNumbers from old space if they
      // can't be taken from new space.
      if (!maybe_value->ToObject(&value)) {
        ASSERT(maybe_value->IsRetryAfterGC() || maybe_value->IsOutOfMemory());
        Heap* heap = from->GetHeap();
        MaybeObject* maybe_value_object =
            heap->AllocateHeapNumber(from->get_scalar(i + from_start),
                                     TENURED);
        if (!maybe_value_object->ToObject(&value)) return maybe_value_object;
      }
      to->set(i + to_start, value, UPDATE_WRITE_BARRIER);
    }
  }
  return to;
}


static void CopyDoubleToDoubleElements(FixedArrayBase* from_base,
                                       uint32_t from_start,
                                       FixedArrayBase* to_base,
                                       uint32_t to_start,
                                       int raw_copy_size) {
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = Min(from_base->length() - from_start,
                    to_base->length() - to_start);
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to_base->length(); ++i) {
        FixedDoubleArray::cast(to_base)->set_the_hole(i);
      }
    }
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
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
                                    FixedArrayBase* to_base,
                                    uint32_t to_start,
                                    int raw_copy_size) {
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from_base->length() - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to_base->length(); ++i) {
        FixedDoubleArray::cast(to_base)->set_the_hole(i);
      }
    }
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
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
                                          uint32_t to_start,
                                          int packed_size,
                                          int raw_copy_size) {
  int copy_size = raw_copy_size;
  uint32_t to_end;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
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
  ASSERT(static_cast<int>(to_end) <= to_base->length());
  ASSERT(packed_size >= 0 && packed_size <= copy_size);
  ASSERT((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
         (copy_size + static_cast<int>(from_start)) <= from_base->length());
  if (copy_size == 0) return;
  FixedArray* from = FixedArray::cast(from_base);
  FixedDoubleArray* to = FixedDoubleArray::cast(to_base);
  for (uint32_t from_end = from_start + static_cast<uint32_t>(packed_size);
       from_start < from_end; from_start++, to_start++) {
    Object* smi = from->get(from_start);
    ASSERT(!smi->IsTheHole());
    to->set(to_start, Smi::cast(smi)->value());
  }
}


static void CopyObjectToDoubleElements(FixedArrayBase* from_base,
                                       uint32_t from_start,
                                       FixedArrayBase* to_base,
                                       uint32_t to_start,
                                       int raw_copy_size) {
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from_base->length() - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to_base->length(); ++i) {
        FixedDoubleArray::cast(to_base)->set_the_hole(i);
      }
    }
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to_base->length() &&
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
  SeededNumberDictionary* from = SeededNumberDictionary::cast(from_base);
  int copy_size = raw_copy_size;
  if (copy_size < 0) {
    ASSERT(copy_size == ElementsAccessor::kCopyToEnd ||
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
    Isolate* isolate = Isolate::Current();
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

  virtual ElementsKind kind() const { return ElementsTraits::Kind; }

  static void ValidateContents(JSObject* holder, int length) {
  }

  static void ValidateImpl(JSObject* holder) {
    FixedArrayBase* fixed_array_base = holder->elements();
    // When objects are first allocated, its elements are Failures.
    if (fixed_array_base->IsFailure()) return;
    if (!fixed_array_base->IsHeapObject()) return;
    Map* map = fixed_array_base->map();
    // Arrays that have been shifted in place can't be verified.
    Heap* heap = holder->GetHeap();
    if (map == heap->one_pointer_filler_map() ||
        map == heap->two_pointer_filler_map() ||
        map == heap->free_space_map()) {
      return;
    }
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

  virtual void Validate(JSObject* holder) {
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
                          FixedArrayBase* backing_store) {
    if (backing_store == NULL) {
      backing_store = holder->elements();
    }
    return ElementsAccessorSubclass::HasElementImpl(
        receiver, holder, key, backing_store);
  }

  MUST_USE_RESULT virtual MaybeObject* Get(Object* receiver,
                                           JSObject* holder,
                                           uint32_t key,
                                           FixedArrayBase* backing_store) {
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
      FixedArrayBase* backing_store) {
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
      FixedArrayBase* backing_store) {
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
      FixedArrayBase* backing_store) {
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

  MUST_USE_RESULT virtual MaybeObject* SetLength(JSArray* array,
                                                 Object* length) {
    return ElementsAccessorSubclass::SetLengthImpl(
        array, length, array->elements());
  }

  MUST_USE_RESULT static MaybeObject* SetLengthImpl(
      JSObject* obj,
      Object* length,
      FixedArrayBase* backing_store);

  MUST_USE_RESULT virtual MaybeObject* SetCapacityAndLength(
      JSArray* array,
      int capacity,
      int length) {
    return ElementsAccessorSubclass::SetFastElementsCapacityAndLength(
        array,
        capacity,
        length);
  }

  MUST_USE_RESULT static MaybeObject* SetFastElementsCapacityAndLength(
      JSObject* obj,
      int capacity,
      int length) {
    UNIMPLEMENTED();
    return obj;
  }

  MUST_USE_RESULT virtual MaybeObject* Delete(JSObject* obj,
                                              uint32_t key,
                                              JSReceiver::DeleteMode mode) = 0;

  MUST_USE_RESULT static MaybeObject* CopyElementsImpl(FixedArrayBase* from,
                                                       uint32_t from_start,
                                                       FixedArrayBase* to,
                                                       ElementsKind from_kind,
                                                       uint32_t to_start,
                                                       int packed_size,
                                                       int copy_size) {
    UNREACHABLE();
    return NULL;
  }

  MUST_USE_RESULT virtual MaybeObject* CopyElements(JSObject* from_holder,
                                                    uint32_t from_start,
                                                    ElementsKind from_kind,
                                                    FixedArrayBase* to,
                                                    uint32_t to_start,
                                                    int copy_size,
                                                    FixedArrayBase* from) {
    int packed_size = kPackedSizeNotKnown;
    if (from == NULL) {
      from = from_holder->elements();
    }

    if (from_holder) {
      bool is_packed = IsFastPackedElementsKind(from_kind) &&
          from_holder->IsJSArray();
      if (is_packed) {
        packed_size = Smi::cast(JSArray::cast(from_holder)->length())->value();
        if (copy_size >= 0 && packed_size > copy_size) {
          packed_size = copy_size;
        }
      }
    }
    return ElementsAccessorSubclass::CopyElementsImpl(
        from, from_start, to, from_kind, to_start, packed_size, copy_size);
  }

  MUST_USE_RESULT virtual MaybeObject* AddElementsToFixedArray(
      Object* receiver,
      JSObject* holder,
      FixedArray* to,
      FixedArrayBase* from) {
    int len0 = to->length();
#ifdef DEBUG
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
      AssertNoAllocation no_gc;
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

  virtual uint32_t GetCapacity(FixedArrayBase* backing_store) {
    return ElementsAccessorSubclass::GetCapacityImpl(backing_store);
  }

  static uint32_t GetKeyForIndexImpl(FixedArrayBase* backing_store,
                                     uint32_t index) {
    return index;
  }

  virtual uint32_t GetKeyForIndex(FixedArrayBase* backing_store,
                                  uint32_t index) {
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
  friend class NonStrictArgumentsElementsAccessor;

  typedef typename KindTraits::BackingStore BackingStore;

  // Adjusts the length of the fast backing store or returns the new length or
  // undefined in case conversion to a slow backing store should be performed.
  static MaybeObject* SetLengthWithoutNormalize(FixedArrayBase* backing_store,
                                                JSArray* array,
                                                Object* length_object,
                                                uint32_t length) {
    uint32_t old_capacity = backing_store->length();
    Object* old_length = array->length();
    bool same_or_smaller_size = old_length->IsSmi() &&
        static_cast<uint32_t>(Smi::cast(old_length)->value()) >= length;
    ElementsKind kind = array->GetElementsKind();

    if (!same_or_smaller_size && IsFastElementsKind(kind) &&
        !IsFastHoleyElementsKind(kind)) {
      kind = GetHoleyElementsKind(kind);
      MaybeObject* maybe_obj = array->TransitionElementsKind(kind);
      if (maybe_obj->IsFailure()) return maybe_obj;
    }

    // Check whether the backing store should be shrunk.
    if (length <= old_capacity) {
      if (array->HasFastSmiOrObjectElements()) {
        MaybeObject* maybe_obj = array->EnsureWritableFastElements();
        if (!maybe_obj->To(&backing_store)) return maybe_obj;
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
          BackingStore::cast(backing_store)->set_the_hole(i);
        }
      }
      return length_object;
    }

    // Check whether the backing store should be expanded.
    uint32_t min = JSObject::NewElementsCapacity(old_capacity);
    uint32_t new_capacity = length > min ? length : min;
    if (!array->ShouldConvertToSlowElements(new_capacity)) {
      MaybeObject* result = FastElementsAccessorSubclass::
          SetFastElementsCapacityAndLength(array, new_capacity, length);
      if (result->IsFailure()) return result;
      array->ValidateElements();
      return length_object;
    }

    // Request conversion to slow elements.
    return array->GetHeap()->undefined_value();
  }

  static MaybeObject* DeleteCommon(JSObject* obj,
                                   uint32_t key,
                                   JSReceiver::DeleteMode mode) {
    ASSERT(obj->HasFastSmiOrObjectElements() ||
           obj->HasFastDoubleElements() ||
           obj->HasFastArgumentsElements());
    Heap* heap = obj->GetHeap();
    Object* elements = obj->elements();
    if (elements == heap->empty_fixed_array()) {
      return heap->true_value();
    }
    typename KindTraits::BackingStore* backing_store =
        KindTraits::BackingStore::cast(elements);
    bool is_non_strict_arguments_elements_map =
        backing_store->map() == heap->non_strict_arguments_elements_map();
    if (is_non_strict_arguments_elements_map) {
      backing_store = KindTraits::BackingStore::cast(
          FixedArray::cast(backing_store)->get(1));
    }
    uint32_t length = static_cast<uint32_t>(
        obj->IsJSArray()
        ? Smi::cast(JSArray::cast(obj)->length())->value()
        : backing_store->length());
    if (key < length) {
      if (!is_non_strict_arguments_elements_map) {
        ElementsKind kind = KindTraits::Kind;
        if (IsFastPackedElementsKind(kind)) {
          MaybeObject* transitioned =
              obj->TransitionElementsKind(GetHoleyElementsKind(kind));
          if (transitioned->IsFailure()) return transitioned;
        }
        if (IsFastSmiOrObjectElementsKind(KindTraits::Kind)) {
          Object* writable;
          MaybeObject* maybe = obj->EnsureWritableFastElements();
          if (!maybe->ToObject(&writable)) return maybe;
          backing_store = KindTraits::BackingStore::cast(writable);
        }
      }
      backing_store->set_the_hole(key);
      // If an old space backing store is larger than a certain size and
      // has too few used values, normalize it.
      // To avoid doing the check on every delete we require at least
      // one adjacent hole to the value being deleted.
      const int kMinLengthForSparsenessCheck = 64;
      if (backing_store->length() >= kMinLengthForSparsenessCheck &&
          !heap->InNewSpace(backing_store) &&
          ((key > 0 && backing_store->is_the_hole(key - 1)) ||
           (key + 1 < length && backing_store->is_the_hole(key + 1)))) {
        int num_used = 0;
        for (int i = 0; i < backing_store->length(); ++i) {
          if (!backing_store->is_the_hole(i)) ++num_used;
          // Bail out early if more than 1/4 is used.
          if (4 * num_used > backing_store->length()) break;
        }
        if (4 * num_used <= backing_store->length()) {
          MaybeObject* result = obj->NormalizeElements();
          if (result->IsFailure()) return result;
        }
      }
    }
    return heap->true_value();
  }

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) {
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
      typename KindTraits::BackingStore* backing_store =
          KindTraits::BackingStore::cast(elements);
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
    case EXTERNAL_BYTE_ARRAY_TYPE:
      return EXTERNAL_BYTE_ELEMENTS;
    case EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
      return EXTERNAL_UNSIGNED_BYTE_ELEMENTS;
    case EXTERNAL_SHORT_ARRAY_TYPE:
      return EXTERNAL_SHORT_ELEMENTS;
    case EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
      return EXTERNAL_UNSIGNED_SHORT_ELEMENTS;
    case EXTERNAL_INT_ARRAY_TYPE:
      return EXTERNAL_INT_ELEMENTS;
    case EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
      return EXTERNAL_UNSIGNED_INT_ELEMENTS;
    case EXTERNAL_FLOAT_ARRAY_TYPE:
      return EXTERNAL_FLOAT_ELEMENTS;
    case EXTERNAL_DOUBLE_ARRAY_TYPE:
      return EXTERNAL_DOUBLE_ELEMENTS;
    case EXTERNAL_PIXEL_ARRAY_TYPE:
      return EXTERNAL_PIXEL_ELEMENTS;
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

  static MaybeObject* CopyElementsImpl(FixedArrayBase* from,
                                       uint32_t from_start,
                                       FixedArrayBase* to,
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
        return to->GetHeap()->undefined_value();
      case FAST_DOUBLE_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
        return CopyDoubleToObjectElements(
            from, from_start, to, to_kind, to_start, copy_size);
      case DICTIONARY_ELEMENTS:
        CopyDictionaryToObjectElements(
            from, from_start, to, to_kind, to_start, copy_size);
        return to->GetHeap()->undefined_value();
      case NON_STRICT_ARGUMENTS_ELEMENTS: {
        // TODO(verwaest): This is a temporary hack to support extending
        // NON_STRICT_ARGUMENTS_ELEMENTS in SetFastElementsCapacityAndLength.
        // This case should be UNREACHABLE().
        FixedArray* parameter_map = FixedArray::cast(from);
        FixedArrayBase* arguments = FixedArrayBase::cast(parameter_map->get(1));
        ElementsKind from_kind = ElementsKindForArray(arguments);
        return CopyElementsImpl(arguments, from_start, to, from_kind,
                                to_start, packed_size, copy_size);
      }
      case EXTERNAL_BYTE_ELEMENTS:
      case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      case EXTERNAL_SHORT_ELEMENTS:
      case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      case EXTERNAL_INT_ELEMENTS:
      case EXTERNAL_UNSIGNED_INT_ELEMENTS:
      case EXTERNAL_FLOAT_ELEMENTS:
      case EXTERNAL_DOUBLE_ELEMENTS:
      case EXTERNAL_PIXEL_ELEMENTS:
        UNREACHABLE();
    }
    return NULL;
  }


  static MaybeObject* SetFastElementsCapacityAndLength(JSObject* obj,
                                                       uint32_t capacity,
                                                       uint32_t length) {
    JSObject::SetFastElementsCapacitySmiMode set_capacity_mode =
        obj->HasFastSmiElements()
            ? JSObject::kAllowSmiElements
            : JSObject::kDontAllowSmiElements;
    return obj->SetFastElementsCapacityAndLength(capacity,
                                                 length,
                                                 set_capacity_mode);
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

  static MaybeObject* SetFastElementsCapacityAndLength(JSObject* obj,
                                                       uint32_t capacity,
                                                       uint32_t length) {
    return obj->SetFastDoubleElementsCapacityAndLength(capacity,
                                                       length);
  }

 protected:
  static MaybeObject* CopyElementsImpl(FixedArrayBase* from,
                                       uint32_t from_start,
                                       FixedArrayBase* to,
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
      case NON_STRICT_ARGUMENTS_ELEMENTS:
      case EXTERNAL_BYTE_ELEMENTS:
      case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      case EXTERNAL_SHORT_ELEMENTS:
      case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      case EXTERNAL_INT_ELEMENTS:
      case EXTERNAL_UNSIGNED_INT_ELEMENTS:
      case EXTERNAL_FLOAT_ELEMENTS:
      case EXTERNAL_DOUBLE_ELEMENTS:
      case EXTERNAL_PIXEL_ELEMENTS:
        UNREACHABLE();
    }
    return to->GetHeap()->undefined_value();
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
template<typename ExternalElementsAccessorSubclass,
         ElementsKind Kind>
class ExternalElementsAccessor
    : public ElementsAccessorBase<ExternalElementsAccessorSubclass,
                                  ElementsKindTraits<Kind> > {
 public:
  explicit ExternalElementsAccessor(const char* name)
      : ElementsAccessorBase<ExternalElementsAccessorSubclass,
                             ElementsKindTraits<Kind> >(name) {}

 protected:
  typedef typename ElementsKindTraits<Kind>::BackingStore BackingStore;

  friend class ElementsAccessorBase<ExternalElementsAccessorSubclass,
                                    ElementsKindTraits<Kind> >;

  MUST_USE_RESULT static MaybeObject* GetImpl(Object* receiver,
                                              JSObject* obj,
                                              uint32_t key,
                                              FixedArrayBase* backing_store) {
    return
        key < ExternalElementsAccessorSubclass::GetCapacityImpl(backing_store)
        ? BackingStore::cast(backing_store)->get(key)
        : backing_store->GetHeap()->undefined_value();
  }

  MUST_USE_RESULT static PropertyAttributes GetAttributesImpl(
      Object* receiver,
      JSObject* obj,
      uint32_t key,
      FixedArrayBase* backing_store) {
    return
        key < ExternalElementsAccessorSubclass::GetCapacityImpl(backing_store)
          ? NONE : ABSENT;
  }

  MUST_USE_RESULT static PropertyType GetTypeImpl(
      Object* receiver,
      JSObject* obj,
      uint32_t key,
      FixedArrayBase* backing_store) {
    return
        key < ExternalElementsAccessorSubclass::GetCapacityImpl(backing_store)
          ? FIELD : NONEXISTENT;
  }

  MUST_USE_RESULT static MaybeObject* SetLengthImpl(
      JSObject* obj,
      Object* length,
      FixedArrayBase* backing_store) {
    // External arrays do not support changing their length.
    UNREACHABLE();
    return obj;
  }

  MUST_USE_RESULT virtual MaybeObject* Delete(JSObject* obj,
                                              uint32_t key,
                                              JSReceiver::DeleteMode mode) {
    // External arrays always ignore deletes.
    return obj->GetHeap()->true_value();
  }

  static bool HasElementImpl(Object* receiver,
                             JSObject* holder,
                             uint32_t key,
                             FixedArrayBase* backing_store) {
    uint32_t capacity =
        ExternalElementsAccessorSubclass::GetCapacityImpl(backing_store);
    return key < capacity;
  }
};


class ExternalByteElementsAccessor
    : public ExternalElementsAccessor<ExternalByteElementsAccessor,
                                      EXTERNAL_BYTE_ELEMENTS> {
 public:
  explicit ExternalByteElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalByteElementsAccessor,
                                 EXTERNAL_BYTE_ELEMENTS>(name) {}
};


class ExternalUnsignedByteElementsAccessor
    : public ExternalElementsAccessor<ExternalUnsignedByteElementsAccessor,
                                      EXTERNAL_UNSIGNED_BYTE_ELEMENTS> {
 public:
  explicit ExternalUnsignedByteElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalUnsignedByteElementsAccessor,
                                 EXTERNAL_UNSIGNED_BYTE_ELEMENTS>(name) {}
};


class ExternalShortElementsAccessor
    : public ExternalElementsAccessor<ExternalShortElementsAccessor,
                                      EXTERNAL_SHORT_ELEMENTS> {
 public:
  explicit ExternalShortElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalShortElementsAccessor,
                                 EXTERNAL_SHORT_ELEMENTS>(name) {}
};


class ExternalUnsignedShortElementsAccessor
    : public ExternalElementsAccessor<ExternalUnsignedShortElementsAccessor,
                                      EXTERNAL_UNSIGNED_SHORT_ELEMENTS> {
 public:
  explicit ExternalUnsignedShortElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalUnsignedShortElementsAccessor,
                                 EXTERNAL_UNSIGNED_SHORT_ELEMENTS>(name) {}
};


class ExternalIntElementsAccessor
    : public ExternalElementsAccessor<ExternalIntElementsAccessor,
                                      EXTERNAL_INT_ELEMENTS> {
 public:
  explicit ExternalIntElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalIntElementsAccessor,
                                 EXTERNAL_INT_ELEMENTS>(name) {}
};


class ExternalUnsignedIntElementsAccessor
    : public ExternalElementsAccessor<ExternalUnsignedIntElementsAccessor,
                                      EXTERNAL_UNSIGNED_INT_ELEMENTS> {
 public:
  explicit ExternalUnsignedIntElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalUnsignedIntElementsAccessor,
                                 EXTERNAL_UNSIGNED_INT_ELEMENTS>(name) {}
};


class ExternalFloatElementsAccessor
    : public ExternalElementsAccessor<ExternalFloatElementsAccessor,
                                      EXTERNAL_FLOAT_ELEMENTS> {
 public:
  explicit ExternalFloatElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalFloatElementsAccessor,
                                 EXTERNAL_FLOAT_ELEMENTS>(name) {}
};


class ExternalDoubleElementsAccessor
    : public ExternalElementsAccessor<ExternalDoubleElementsAccessor,
                                      EXTERNAL_DOUBLE_ELEMENTS> {
 public:
  explicit ExternalDoubleElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalDoubleElementsAccessor,
                                 EXTERNAL_DOUBLE_ELEMENTS>(name) {}
};


class PixelElementsAccessor
    : public ExternalElementsAccessor<PixelElementsAccessor,
                                      EXTERNAL_PIXEL_ELEMENTS> {
 public:
  explicit PixelElementsAccessor(const char* name)
      : ExternalElementsAccessor<PixelElementsAccessor,
                                 EXTERNAL_PIXEL_ELEMENTS>(name) {}
};


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

  MUST_USE_RESULT static MaybeObject* DeleteCommon(
      JSObject* obj,
      uint32_t key,
      JSReceiver::DeleteMode mode) {
    Isolate* isolate = obj->GetIsolate();
    Heap* heap = isolate->heap();
    FixedArray* backing_store = FixedArray::cast(obj->elements());
    bool is_arguments =
        (obj->GetElementsKind() == NON_STRICT_ARGUMENTS_ELEMENTS);
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

  MUST_USE_RESULT static MaybeObject* CopyElementsImpl(FixedArrayBase* from,
                                                       uint32_t from_start,
                                                       FixedArrayBase* to,
                                                       ElementsKind from_kind,
                                                       uint32_t to_start,
                                                       int packed_size,
                                                       int copy_size) {
    UNREACHABLE();
    return NULL;
  }


 protected:
  friend class ElementsAccessorBase<DictionaryElementsAccessor,
                                    ElementsKindTraits<DICTIONARY_ELEMENTS> >;

  MUST_USE_RESULT virtual MaybeObject* Delete(JSObject* obj,
                                              uint32_t key,
                                              JSReceiver::DeleteMode mode) {
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


class NonStrictArgumentsElementsAccessor : public ElementsAccessorBase<
    NonStrictArgumentsElementsAccessor,
    ElementsKindTraits<NON_STRICT_ARGUMENTS_ELEMENTS> > {
 public:
  explicit NonStrictArgumentsElementsAccessor(const char* name)
      : ElementsAccessorBase<
          NonStrictArgumentsElementsAccessor,
          ElementsKindTraits<NON_STRICT_ARGUMENTS_ELEMENTS> >(name) {}
 protected:
  friend class ElementsAccessorBase<
      NonStrictArgumentsElementsAccessor,
      ElementsKindTraits<NON_STRICT_ARGUMENTS_ELEMENTS> >;

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

  MUST_USE_RESULT static MaybeObject* SetLengthImpl(
      JSObject* obj,
      Object* length,
      FixedArrayBase* parameter_map) {
    // TODO(mstarzinger): This was never implemented but will be used once we
    // correctly implement [[DefineOwnProperty]] on arrays.
    UNIMPLEMENTED();
    return obj;
  }

  MUST_USE_RESULT virtual MaybeObject* Delete(JSObject* obj,
                                              uint32_t key,
                                              JSReceiver::DeleteMode mode) {
    FixedArray* parameter_map = FixedArray::cast(obj->elements());
    Object* probe = GetParameterMapArg(obj, parameter_map, key);
    if (!probe->IsTheHole()) {
      // TODO(kmillikin): We could check if this was the last aliased
      // parameter, and revert to normal elements in that case.  That
      // would enable GC of the context.
      parameter_map->set_the_hole(key + 2);
    } else {
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      if (arguments->IsDictionary()) {
        return DictionaryElementsAccessor::DeleteCommon(obj, key, mode);
      } else {
        // It's difficult to access the version of DeleteCommon that is declared
        // in the templatized super class, call the concrete implementation in
        // the class for the most generalized ElementsKind subclass.
        return FastHoleyObjectElementsAccessor::DeleteCommon(obj, key, mode);
      }
    }
    return obj->GetHeap()->true_value();
  }

  MUST_USE_RESULT static MaybeObject* CopyElementsImpl(FixedArrayBase* from,
                                                       uint32_t from_start,
                                                       FixedArrayBase* to,
                                                       ElementsKind from_kind,
                                                       uint32_t to_start,
                                                       int packed_size,
                                                       int copy_size) {
    UNREACHABLE();
    return NULL;
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
MUST_USE_RESULT MaybeObject* ElementsAccessorBase<ElementsAccessorSubclass,
                                                  ElementsKindTraits>::
    SetLengthImpl(JSObject* obj,
                  Object* length,
                  FixedArrayBase* backing_store) {
  JSArray* array = JSArray::cast(obj);

  // Fast case: The new length fits into a Smi.
  MaybeObject* maybe_smi_length = length->ToSmi();
  Object* smi_length = Smi::FromInt(0);
  if (maybe_smi_length->ToObject(&smi_length) && smi_length->IsSmi()) {
    const int value = Smi::cast(smi_length)->value();
    if (value >= 0) {
      Object* new_length;
      MaybeObject* result = ElementsAccessorSubclass::
          SetLengthWithoutNormalize(backing_store, array, smi_length, value);
      if (!result->ToObject(&new_length)) return result;
      ASSERT(new_length->IsSmi() || new_length->IsUndefined());
      if (new_length->IsSmi()) {
        array->set_length(Smi::cast(new_length));
        return array;
      }
    } else {
      return ThrowArrayLengthRangeError(array->GetHeap());
    }
  }

  // Slow case: The new length does not fit into a Smi or conversion
  // to slow elements is needed for other reasons.
  if (length->IsNumber()) {
    uint32_t value;
    if (length->ToArrayIndex(&value)) {
      SeededNumberDictionary* dictionary;
      MaybeObject* maybe_object = array->NormalizeElements();
      if (!maybe_object->To(&dictionary)) return maybe_object;
      Object* new_length;
      MaybeObject* result = DictionaryElementsAccessor::
          SetLengthWithoutNormalize(dictionary, array, length, value);
      if (!result->ToObject(&new_length)) return result;
      ASSERT(new_length->IsNumber());
      array->set_length(new_length);
      return array;
    } else {
      return ThrowArrayLengthRangeError(array->GetHeap());
    }
  }

  // Fall-back case: The new length is not a number so make the array
  // size one and set only element to length.
  FixedArray* new_backing_store;
  MaybeObject* maybe_obj = array->GetHeap()->AllocateFixedArray(1);
  if (!maybe_obj->To(&new_backing_store)) return maybe_obj;
  new_backing_store->set(0, length);
  { MaybeObject* result = array->SetContent(new_backing_store);
    if (result->IsFailure()) return result;
  }
  return array;
}


MUST_USE_RESULT MaybeObject* ArrayConstructInitializeElements(
    JSArray* array, Arguments* args) {
  Heap* heap = array->GetIsolate()->heap();

  // Optimize the case where there is one argument and the argument is a
  // small smi.
  if (args->length() == 1) {
    Object* obj = (*args)[0];
    if (obj->IsSmi()) {
      int len = Smi::cast(obj)->value();
      if (len > 0 && len < JSObject::kInitialMaxFastElementArray) {
        ElementsKind elements_kind = array->GetElementsKind();
        MaybeObject* maybe_array = array->Initialize(len, len);
        if (maybe_array->IsFailure()) return maybe_array;

        if (!IsFastHoleyElementsKind(elements_kind)) {
          elements_kind = GetHoleyElementsKind(elements_kind);
          maybe_array = array->TransitionElementsKind(elements_kind);
          if (maybe_array->IsFailure()) return maybe_array;
        }

        return array;
      } else if (len == 0) {
        return array->Initialize(JSArray::kPreallocatedArrayElements);
      }
    }

    // Take the argument as the length.
    MaybeObject* maybe_obj = array->Initialize(0);
    if (!maybe_obj->To(&obj)) return maybe_obj;

    return array->SetElementsLength((*args)[0]);
  }

  // Optimize the case where there are no parameters passed.
  if (args->length() == 0) {
    return array->Initialize(JSArray::kPreallocatedArrayElements);
  }

  // Set length and elements on the array.
  int number_of_elements = args->length();
  MaybeObject* maybe_object =
      array->EnsureCanContainElements(args, 0, number_of_elements,
                                      ALLOW_CONVERTED_DOUBLE_ELEMENTS);
  if (maybe_object->IsFailure()) return maybe_object;

  // Allocate an appropriately typed elements array.
  MaybeObject* maybe_elms;
  ElementsKind elements_kind = array->GetElementsKind();
  if (IsFastDoubleElementsKind(elements_kind)) {
    maybe_elms = heap->AllocateUninitializedFixedDoubleArray(
        number_of_elements);
  } else {
    maybe_elms = heap->AllocateFixedArrayWithHoles(number_of_elements);
  }
  FixedArrayBase* elms;
  if (!maybe_elms->To(&elms)) return maybe_elms;

  // Fill in the content
  switch (array->GetElementsKind()) {
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_SMI_ELEMENTS: {
      FixedArray* smi_elms = FixedArray::cast(elms);
      for (int index = 0; index < number_of_elements; index++) {
        smi_elms->set(index, (*args)[index], SKIP_WRITE_BARRIER);
      }
      break;
    }
    case FAST_HOLEY_ELEMENTS:
    case FAST_ELEMENTS: {
      AssertNoAllocation no_gc;
      WriteBarrierMode mode = elms->GetWriteBarrierMode(no_gc);
      FixedArray* object_elms = FixedArray::cast(elms);
      for (int index = 0; index < number_of_elements; index++) {
        object_elms->set(index, (*args)[index], mode);
      }
      break;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      FixedDoubleArray* double_elms = FixedDoubleArray::cast(elms);
      for (int index = 0; index < number_of_elements; index++) {
        double_elms->set(index, (*args)[index]->Number());
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }

  array->set_elements(elms);
  array->set_length(Smi::FromInt(number_of_elements));
  return array;
}

} }  // namespace v8::internal
